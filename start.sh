#!/bin/bash

cd ./src
make
cd ..
chown root main

if [ ! -f web_main.db ]; then
	cd ./web
	flask --app=web_main init_db
	chown shaughn ../web_main.db
else
	cd ./web
fi

sudo python web_main.py
