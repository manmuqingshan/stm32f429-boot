#!/bin/bash

gcc imgpack.c load.c cJSON/cJSON.c -I./ -I./cJSON -o imgpack