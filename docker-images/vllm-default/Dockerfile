FROM vllm/vllm-openai:latest

FROM vllm/vllm-openai:latest

# Install git so uv can pull from GitHub
RUN apt-get update && apt-get install -y git && rm -rf /var/lib/apt/lists/*

# Now install the development version of transformers
RUN uv pip install --system git+https://github.com/huggingface/transformers.git
