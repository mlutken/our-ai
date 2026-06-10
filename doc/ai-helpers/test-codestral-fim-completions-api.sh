#!/bin/bash


# #  https://codestral.mistral.ai/v1/fim/completions

curl -X POST https://api.mistral.ai/v1/fim/completions \
  -H "Authorization: Bearer ${MISTRAL_API_KEY}" \
  -H "Content-Type: application/json" \
  -d '{"model": "codestral-latest", "prompt": "std::vec"}'

