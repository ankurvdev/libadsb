#!/bin/bash
rtl_sdr -f 978000000 -s 2083334 -g 48 -d 1 - | tee rtlsdr_978_`date +'%Y-%m-%d-%H-%M-%S'`.dat | ~/dump978 | ~/uat2text 

