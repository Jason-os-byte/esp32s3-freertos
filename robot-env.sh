#!/usr/bin/env bash
export ROBOT_ROOT=$HOME/project/esp32s3_freeRTOS
export PORT=${PORT:-/dev/ttyACM0}
export IDF_SANITY_DIR=$ROBOT_ROOT/idf_sanity/hello_world
export PORT_LAB=$ROBOT_ROOT/freertos-port-lab
export APP_OFFSET=${APP_OFFSET:-0x10000}