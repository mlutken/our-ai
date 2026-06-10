#!/usr/bin/python
import requests
import sys
import json
import os

API_KEY = os.environ["MISTRAL_API_KEY"]  # Replace with your key
MODEL = "mistral-tiny"     # Or "mistral-small", "mistral-medium"

def query_mistral(prompt):
    url = "https://api.mistral.ai/v1/chat/completions"
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }
    payload = {
        "model": MODEL,
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.7
    }
    response = requests.post(url, headers=headers, json=payload)
    return response.json()["choices"][0]["message"]["content"]

if __name__ == "__main__":
    code_snippet = sys.stdin.read()
    prompt = f"Improve this C++23 code:\n{code_snippet}"
    print(query_mistral(prompt))
