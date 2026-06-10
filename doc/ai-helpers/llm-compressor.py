import argparse
from compressed_tensors.offload import dispatch_model
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer

from llmcompressor import oneshot
from llmcompressor.modifiers.quantization import QuantizationModifier
from llmcompressor.modifiers.gptq import GPTQModifier
import os
import torch

# https://docs.vllm.ai/en/latest/features/quantization/

os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

# Command line arguments setup
parser = argparse.ArgumentParser(
    description="Quantize an LLM to INT8, INT4, FP8, FP4, or MARLIN using llm-compressor."
)
parser.add_argument(
    "--model",
    type=str,
    default="Qwen/Qwen2-1.5B",
    help="Hugging Face model ID (e.g., Qwen/Qwen2-1.5B or a local path)",
)
# Tilføjet "marlin" i choices
parser.add_argument(
    "--format",
    type=str,
    default="int8",
    choices=["int8", "int4", "fp8", "fp4", "marlin"],
    help="Quantization format to use: 'int8', 'int4', 'fp8', 'fp4', or 'marlin'"
)
parser.add_argument( "--test", type=str, default="none", choices=["none", "cpu", "gpu"], help="Run quick test of model after. Options: [none], cpu, gpu")
parser.add_argument( "--samples", type=int, default=128, help="Calibration samples count fx: '128', '256', '512'")
parser.add_argument( "--seqlen", type=int, default=1024, help="Sequence length to use during compression fx: '512', '1024', '2048'")

args = parser.parse_args()

MODEL_ID = args.model
QUANT_FORMAT = args.format.lower()

# 1) Load model and tokenizer
print(f"Loading model: {MODEL_ID}")
model = AutoModelForCausalLM.from_pretrained(MODEL_ID, torch_dtype="auto")
tokenizer = AutoTokenizer.from_pretrained(MODEL_ID)

# 2) Prepare calibration dataset
DATASET_ID = "HuggingFaceH4/ultrachat_200k"
DATASET_SPLIT = "train_sft"

NUM_CALIBRATION_SAMPLES = args.samples
MAX_SEQUENCE_LENGTH = args.seqlen

print(f"seqlen : {args.seqlen}")
print(f"Fetching calibration data from: {DATASET_ID}")
ds = load_dataset(DATASET_ID, split=f"{DATASET_SPLIT}[:{NUM_CALIBRATION_SAMPLES}]")
ds = ds.shuffle(seed=42)

# Preprocess dataset using the chat template
def preprocess(example):
    return {
        "text": tokenizer.apply_chat_template(
            example["messages"],
            tokenize=False,
            add_generation_prompt=False,
        )
    }

ds = ds.map(preprocess)

# Tokenize inputs
def tokenize(sample):
    return tokenizer(
        sample["text"],
        padding=False,
        max_length=MAX_SEQUENCE_LENGTH,
        truncation=True,
        add_special_tokens=False,
    )

ds = ds.map(tokenize, remove_columns=ds.column_names)

# 3) Configure quantization recipe based on the chosen format
if QUANT_FORMAT == "marlin":
    print("Configuring GPTQ recipe optimized for MARLIN kernels (W4A16, group_size=128)...")
    # Marlin kræver specifikt W4A16 med group_size=128
    recipe = GPTQModifier(
        targets="Linear",
        scheme="W4A16",
        group_size=128,
        ignore=["lm_head"]
    )

elif QUANT_FORMAT == "int4":
    print("Configuring GPTQ recipe for INT4 (W4A16)...")
    recipe = GPTQModifier(targets="Linear", scheme="W4A16", ignore=["lm_head"])

elif QUANT_FORMAT == "fp8":
    print("Configuring Quantization recipe for FP8 (FP8_DYNAMIC)...")
    recipe = QuantizationModifier(targets="Linear", scheme="FP8_DYNAMIC", ignore=["lm_head"])

elif QUANT_FORMAT == "fp4":
    print("Configuring Quantization recipe for FP4 (NVFP4)...")
    recipe = QuantizationModifier(targets="Linear", scheme="NVFP4", ignore=["lm_head"])

else:
    print("Configuring GPTQ recipe for INT8 (W8A8)...")
    recipe = GPTQModifier(targets="Linear", scheme="W8A8", ignore=["lm_head"])

# 4) Apply quantization
torch.cuda.empty_cache() # Tømmer ubrugt VRAM-cache
print(f"Starting {QUANT_FORMAT.upper()} quantization (oneshot)...")
oneshot(
    model=model,
    dataset=ds,
    recipe=recipe,
    max_seq_length=MAX_SEQUENCE_LENGTH,
    num_calibration_samples=NUM_CALIBRATION_SAMPLES,
)

# Confirm generations of the quantized model look sane
# RETTET: Tjekker nu args.test i stedet for args.samples
if args.test != "none":
    print("========== SAMPLE GENERATION ==============")
    if args.test == "cpu":
        input_ids = tokenizer("Hello my name is", return_tensors="pt").input_ids
    else: # gpu
        dispatch_model(model)
        input_ids = tokenizer("Hello my name is", return_tensors="pt").input_ids.to(model.device)

    output = model.generate(input_ids, max_new_tokens=20, disable_compile=True)
    print(tokenizer.decode(output[0]))
    print("==========================================")

# 5) Save to disk in compressed-tensors format
folder_name = MODEL_ID.rstrip("/").split("/")[-1]
SAVE_DIR = f"{folder_name}-{QUANT_FORMAT.upper()}"

print(f"Saving quantized model to: {SAVE_DIR}")
model.save_pretrained(SAVE_DIR, save_compressed=True)
tokenizer.save_pretrained(SAVE_DIR)
print("Done!")
