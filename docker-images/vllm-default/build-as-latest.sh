#!/bin/bash

SAVE_CUR_DIR="$( pwd )"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${SCRIPT_DIR}

docker build -t vllm-default:latest .

cd ${SAVE_CUR_DIR}
