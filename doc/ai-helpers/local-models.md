python -m vllm.entrypoints.openai.api_server --model mistralai/Ministral-3B  --quantization bitsandbytes --dtype float16 --tensor-parallel-size 1 --port 8001
python -m vllm.entrypoints.openai.api_server --model mistralai/Mistral-7B-Instruct-v0.1 --quantization bitsandbytes --dtype auto --tensor-parallel-size 1 --port 8001
python -m vllm.entrypoints.openai.api_server --model mistralai/Mistral-7B-v0.1 --quantization bitsandbytes --dtype auto --tensor-parallel-size 1 --max-model-len 2048  --port 8001
VLLM_MEMORY_PROFILER_ESTIMATE_CUDAGRAPHS=0 python -m vllm.entrypoints.openai.api_server --model mistralai/Mistral-7B-v0.1 --quantization bitsandbytes --dtype auto --tensor-parallel-size 1 --max-model-len 2048 --gpu-memory-utilization 0.7 --port 8001 --chat-template "{% if messages[0]['role'] == 'system' %}{{ messages[0]['content'] + '\n' }}{% endif %}{% for message in messages[1:] %}{{'<s>' + message['role'] + '\n' + message['content'] + '\n'}}{% endfor %}{% if add_generation_prompt %}{{ '<s>' + 'assistant' + '\n' }}{% endif %}"

python -m vllm.entrypoints.openai.api_server --model TinyLlama/TinyLlama-1.1B --quantization bitsandbytes --dtype auto --tensor-parallel-size 1 --port 8001


Mistral-7B-Instruct-v0.1 ()
==========================

start the server
----------------

VLLM_MEMORY_PROFILER_ESTIMATE_CUDAGRAPHS=0 python -m vllm.entrypoints.openai.api_server \
    --model mistralai/Mistral-7B-Instruct-v0.1 \
    --chat-template "{{ '[INST] ' + messages[-1]['content'] + ' [/INST]' }}" \
    --quantization bitsandbytes \
    --dtype auto \
    --tensor-parallel-size 1 \
    --max-model-len 2048 \
    --gpu-memory-utilization 0.7 \
    --port 8001





Testing out the vllm model
--------------------------
curl -s http://localhost:8001/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "mistralai/Mistral-7B-v0.1",
    "prompt": "What is the capital of France?",
    "max_tokens": 50,
    "temperature": 0.7
  }' | jq -r '.choices[0].text'



  Mistral-7B-Instruct-v0.1 (chat mode)
======================================

start the server
----------------

VLLM_MEMORY_PROFILER_ESTIMATE_CUDAGRAPHS=0 python -m vllm.entrypoints.openai.api_server \
    --model mistralai/Mistral-7B-Instruct-v0.1 \
    --chat-template "{{ '<s>' + message['role'] + '\n' + message['content'] + '\n' }}"  \
    --quantization bitsandbytes \
    --dtype auto \
    --tensor-parallel-size 1 \
    --max-model-len 2048 \
    --gpu-memory-utilization 0.7 \
    --port 8001





Testing out the vllm model
--------------------------
curl -s http://localhost:8001/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "mistralai/Mistral-7B-v0.1",
    "messages": [{"role": "user", "content": "What is the capital of France?"}],
    "max_tokens": 50
  }' | jq -r '.choices[0].message.content'

