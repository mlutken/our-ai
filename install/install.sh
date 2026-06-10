#!/bin/bash
ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Default values
ENV_FILE=""
RE_INSTALL="n"

# Define the usage message
usage() {
    echo "Options:"
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

setup_librechat() {

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


echo "ENV_FILE            : '${ENV_FILE}'"
echo "RE_INSTALL          : '${RE_INSTALL}'"

if [ "y" == "${RE_INSTALL}" ]
then
    echo "Doing a complete reinstall...."
fi



