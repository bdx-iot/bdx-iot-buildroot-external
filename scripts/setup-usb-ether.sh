#!/bin/bash

sudo ip addr flush dev $1
sudo ip addr add $2/24 dev $1

