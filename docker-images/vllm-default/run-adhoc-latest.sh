#!/bin/bash

# docker run --gpus all   -v ~/.cache/huggingface:/root/.cache/huggingface --env "HF_TOKEN=$HF_TOKEN" -p 8000:8000 --ipc=host vllm-default:latest Qwen/Qwen2.5-7B-Instruct-AWQ --quantization awq --gpu-memory-utilization 0.8 --max-model-len 4096 --enable-auto-tool-choice --tool-call-parser pythonic --trust-remote-code --host 0.0.0.0 --port 8000 --served-model-name default

docker run --gpus all   -v ~/.cache/huggingface:/root/.cache/huggingface --env "HF_TOKEN=$HF_TOKEN" -p 8000:8000 --ipc=host vllm-default:latest Qwen/Qwen2.5-7B-Instruct-AWQ --quantization awq --gpu-memory-utilization 0.8 --max-model-len 4096 --enable-auto-tool-choice --tool-call-parser pythonic --trust-remote-code --host 0.0.0.0 --port 8000 --served-model-name default

