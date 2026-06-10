#!/bin/bash


curl -X POST https://api.mistral.ai/v1/chat/completions \
  -H "Authorization: Bearer ${MISTRAL_API_KEY}" \
  -H "Content-Type: application/json" \
  -d '{"model": "codestral-latest", "messages": [{"role": "user", "content": "std::vec"}]}'

