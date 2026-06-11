#!/bin/bash
REPO_ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"

# Default values
CONFIG_NAME="standard"
ENV_FILE=""
RE_INSTALL="n"

# Define the usage message
usage() {
    echo "Options:"
    echo "  -c=|--config=[${CONFIG_NAME}]"
    echo "    Configuration name to use. See Config files in ${REPO_ROOT_DIR}/config directory!"
    echo " "
    echo "  -e=|--env=[${ENV_FILE}]"
    echo "    Environment file to use."
    echo " "
    echo "  -r|--reinstall"
    echo "    Reinstall all from scratch."
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
TEMP=$(getopt -o hre: --long help,reinstall,env: -n "$0" -- "$@")

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
        -r | --reinstall)
            RE_INSTALL="y"
            shift 1
            ;;
        -e | --env)
            ENV_FILE="$2"
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


echo "REPO_ROOT_DIR       : '${REPO_ROOT_DIR}'"
echo "ENV_FILE            : '${ENV_FILE}'"
echo "RE_INSTALL          : '${RE_INSTALL}'"

# See: https://www.librechat.ai/docs/local/docker
#      https://www.librechat.ai/docs/remote/docker_linux
install_librechat() {
    if [ -d ${REPO_ROOT_DIR}/3rdparty/LibreChat ]; then
        cd ${REPO_ROOT_DIR}/3rdparty/LibreChat && docker compose down ;  cd ${REPO_ROOT_DIR}
    fi

    if [ "y" == "${RE_INSTALL}" ]; then
        echo "Removing LibreChat ..."
        rm -rf ${REPO_ROOT_DIR}/3rdparty/LibreChat
    fi

    if [ ! -d ${REPO_ROOT_DIR}/3rdparty/LibreChat ]; then
        echo "Installing LibreChat ..."
        cd ${REPO_ROOT_DIR}/3rdparty
        git clone https://github.com/danny-avila/LibreChat.git

    fi
    echo "Updating LibreChat config ..."
    cd ${REPO_ROOT_DIR}/3rdparty/LibreChat
    cp .env.example .env
    cp ${REPO_ROOT_DIR}/config/LibreChat/base/* .
    cp ${REPO_ROOT_DIR}/config/LibreChat/${CONFIG_NAME}/* .
    docker compose down
    docker compose up -d
    cd ${REPO_ROOT_DIR}
}

install_docker() {
    if ! command -v docker &> /dev/null; then
        echo "Installing docker "
        sudo apt update
        sudo apt install -y apt-transport-https ca-certificates curl software-properties-common gnupg lsb-release

        curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
        echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
        sudo apt update

        sudo apt install -y docker-ce
        sudo usermod -aG docker $USER
    else
        echo "Docker already installed!"
    fi
}


build_docker_images() {
    ${REPO_ROOT_DIR}/docker-images/vllm-default/build-as-latest.sh
    cd ${REPO_ROOT_DIR}
}


if [ "y" == "${RE_INSTALL}" ]
then
    echo "Doing a complete reinstall...."
fi

install_docker
build_docker_images
# install_librechat




