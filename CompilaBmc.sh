#!/bin/sh -xe

case $1 in
	bmc)
		echo 'Compiling bmc .....!!!!!'
		./bootstrap.sh
		./configure ${CONFIGURE_FLAGS}
		make
		exit 0
		;;


	clean)
		echo 'Cleaning bmc .....!!!!!'
		./bootstrap.sh clean
		exit 0
		;;

	*)
		exit 0
		;;
esac
