#!/bin/bash

echo "MISTRAL_API_KEY: ${MISTRAL_API_KEY}"

curl -X GET https://api.mistral.ai/v1/models \
  -H "Authorization: Bearer ${MISTRAL_API_KEY}" \
  -H "Content-Type: application/json" | jq

