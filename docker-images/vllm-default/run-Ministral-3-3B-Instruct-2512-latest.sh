#!/bin/bash

docker run --gpus all -v ~/.cache/huggingface:/root/.cache/huggingface --env "HF_TOKEN=$HF_TOKEN" -p 8000:8000 --ipc=host vllm-default:latest mistralai/Ministral-3-3B-Instruct-2512 --gpu-memory-utilization 0.8 --max-model-len 9000 --enable-auto-tool-choice --tool-call-parser pythonic --trust-remote-code --host 0.0.0.0 --port 8000 --served-model-name default --language-model-only
