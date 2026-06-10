#!/usr/bin/python
import requests
import sys
import json
import os

import sys

# Debug: Print all input and environment
print("=== DEBUG INFO ===", file=sys.stderr)
print(f"Input from stdin: {sys.stdin.read()}", file=sys.stderr)
print(f"Arguments: {sys.argv}", file=sys.stderr)
print("===================", file=sys.stderr)

API_KEY = os.environ["MISTRAL_API_KEY"]  # Replace with your key


if __name__ == "__main__":
    code_snippet = sys.stdin.read()
    print(f"MISTRAL_API_KEY: {API_KEY}\n{code_snippet}\n{code_snippet}\n")
