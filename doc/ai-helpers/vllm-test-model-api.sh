#!/bin/bash
ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Default values
PROMPT="What is the capital of France?"
MODEL="default"
BASE_URL="http://localhost:8000"
API_KEY="${MISTRAL_API_KEY}"
USE_JQ="n"

# Define the usage message
usage() {
    echo "Options:"
    echo "  -p=|--prompt=[{$PROMPT}]"
    echo "    Prompt to send. For example 'Write a C++ hello world program'."
    echo " "
    echo "  -m=|--model=[{$MODEL}]"
    echo "    Model to test. For example 'default', 'Qwen/Qwen2-1.5B' or 'mistral-large-latest'."
    echo " "
    echo "  -u=|--base-url=[{$BASE_URL}]"
    echo "    Base URL to use. For example 'http://localhost:8000' or 'https://api.mistral.ai'."
    echo " "
    echo "  -k=|--api-keyl=[{$API_KEY}]"
    echo "    API key to use if needed."
    echo " "
    echo "  -j|--jq"
    echo "    Pipe result through jq for nicer json formatting."
    echo " "
    echo "  -h=|--help"
    echo "    Print this help"
    echo " "
    exit 1
}



# Parse options using getopt
# The first argument to getopt is the argument string (e.g., "hp:n:")
# The second argument is the list of long options (e.g., "help,port:,name:")
# The colon (:) after an option means it requires an argument.
TEMP=$(getopt -o hjp:m:u:k: --long help,jq,prompt:,model:,base-url:,api-key: -n "$0" -- "$@")

# Check if getopt failed
if [ $? -ne 0 ]; then
    echo "Error: Failed to parse arguments" >&2
    usage
fi

# Evaluate the parsed arguments
eval set -- "$TEMP"

# Extract options and their arguments
while true; do
    case "$1" in
        -h | --help)
            usage
            ;;
        -j | --jq)
            USE_JQ="y"
            shift 1
            ;;
        -p | --prompt)
            PROMPT="$2"
            shift 2
            ;;
        -m | --model)
            MODEL="$2"
            shift 2
            ;;
        -u | --base-url)
            BASE_URL="$2"
            shift 2
            ;;
        -k | --api-key)
            API_KEY="$2"
            shift 2
            ;;
        --)
            shift      # End of options
            break
            ;;
        *)
            echo "Error: Unknown option $1" >&2
            usage
            ;;
    esac
done


echo "PROMPT              : '${PROMPT}'"
echo "MODEL               : '${MODEL}'"
echo "BASE_URL            : '${BASE_URL}'"
echo "API_KEY             : '${API_KEY}'"
echo "USE_JQ              : '${USE_JQ}'"

if [ "y" == "${USE_JQ}" ]
then
    curl -X POST ${BASE_URL}/v1/chat/completions \
    -H "Authorization: Bearer ${API_KEY}" \
    -H "Content-Type: application/json" \
    -d "{\"model\": \"${MODEL}\", \"messages\": [{\"role\": \"user\", \"content\": \"${PROMPT}\"}]}" | jq
else
    curl -X POST ${BASE_URL}/v1/chat/completions \
    -H "Authorization: Bearer ${API_KEY}" \
    -H "Content-Type: application/json" \
    -d "{\"model\": \"${MODEL}\", \"messages\": [{\"role\": \"user\", \"content\": \"${PROMPT}\"}]}"
fi



