import argparse
from compressed_tensors.offload import dispatch_model
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer

from llmcompressor import oneshot
from llmcompressor.modifiers.gptq import GPTQModifier

# Command line arguments setup
parser = argparse.ArgumentParser(
    description="Quantize an LLM to W8A8 INT8 using llm-compressor."
)
parser.add_argument(
    "--model",
    type=str,
    default="Qwen/Qwen2-1.5B",
    help="Hugging Face model ID (e.g., Qwen/Qwen2-1.5B or a local path)",
)
args = parser.parse_args()

# 1) Select model and load it based on the input argument
MODEL_ID = args.model
print(f"Loading model: {MODEL_ID}")

model = AutoModelForCausalLM.from_pretrained(MODEL_ID, dtype="auto")
tokenizer = AutoTokenizer.from_pretrained(MODEL_ID)

# 2) Prepare calibration dataset
DATASET_ID = "HuggingFaceH4/ultrachat_200k"
DATASET_SPLIT = "train_sft"

# Select number of samples. 512 samples is a good place to start.
# Increasing the number of samples can improve accuracy.
NUM_CALIBRATION_SAMPLES = 512
MAX_SEQUENCE_LENGTH = 2048

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

# 3) Select quantization algorithms. In this case, we:
#   * quantize the weights to int8 with GPTQ (static per channel)
#   * quantize the activations to int8 (dynamic per token)
recipe = GPTQModifier(targets="Linear", scheme="W8A8", ignore=["lm_head"])

# 4) Apply quantization
print("Starting quantization (oneshot)...")
oneshot(
    model=model,
    dataset=ds,
    recipe=recipe,
    max_seq_length=MAX_SEQUENCE_LENGTH,
    num_calibration_samples=NUM_CALIBRATION_SAMPLES,
)

# Confirm generations of the quantized model look sane
print("========== SAMPLE GENERATION ==============")
dispatch_model(model)
input_ids = tokenizer("Hello my name is", return_tensors="pt").input_ids.to(
    model.device
)
output = model.generate(input_ids, max_new_tokens=20, disable_compile=True)
print(tokenizer.decode(output[0]))
print("==========================================")

# 5) Save to disk in compressed-tensors format
# Handles both full Hugging Face IDs (e.g., Qwen/Qwen2-1.5B) and local paths
folder_name = MODEL_ID.rstrip("/").split("/")[-1]
SAVE_DIR = f"{folder_name}-INT8"

print(f"Saving quantized model to: {SAVE_DIR}")
model.save_pretrained(SAVE_DIR, save_compressed=True)
tokenizer.save_pretrained(SAVE_DIR)
print("Done!")
