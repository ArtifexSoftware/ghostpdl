#!/bin/bash

# This script executes the install step when running under travis-ci

#if cygwin, check path
case ${MACHTYPE} in
	*cygwin*) OPJ_CI_IS_CYGWIN=1;;
	*) ;;
esac

if [ "${OPJ_CI_IS_CYGWIN:-}" == "1" ]; then
	# PATH is not yet set up
	export PATH=$PATH:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
fi

# Set-up some error handling
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value
set -o pipefail  ## Fail on error in pipe

function exit_handler ()
{
	local exit_code="$?"
	
	test ${exit_code} == 0 && return;

	echo -e "\nInstall failed !!!\nLast command at line ${BASH_LINENO}: ${BASH_COMMAND}";
	exit "${exit_code}"
}
trap exit_handler EXIT
trap exit ERR

# We don't need anything for coverity scan builds. ABI check is managed in abi-check.sh
if [ "${COVERITY_SCAN_BRANCH:-}" == "1" ] || [ "${OPJ_CI_ABI_CHECK:-}" == "1" ]; then
	exit 0
fi

if [ "${OPJ_CI_SKIP_TESTS:-}" != "1" ]; then

	OPJ_SOURCE_DIR=$(cd $(dirname $0)/../.. && pwd)

	# We need test data
	if [ "${GITHUB_HEAD_REF:-}" != "" ]; then
		OPJ_DATA_BRANCH=${GITHUB_HEAD_REF}
	elif [ "${TRAVIS_BRANCH:-}" != "" ]; then
		OPJ_DATA_BRANCH=${TRAVIS_BRANCH}
	elif [ "${APPVEYOR_REPO_BRANCH:-}" != "" ]; then
		OPJ_DATA_BRANCH=${APPVEYOR_REPO_BRANCH}
	else
		OPJ_DATA_BRANCH=$(git -C ${OPJ_SOURCE_DIR} branch | grep '*' | tr -d '*[[:blank:]]') #default to same branch as we're setting up
	fi
	OPJ_DATA_HAS_BRANCH=$(git ls-remote --heads https://github.com/uclouvain/openjpeg-data ${OPJ_DATA_BRANCH} | wc -l)
	if [ ${OPJ_DATA_HAS_BRANCH} -eq 0 ]; then
		OPJ_DATA_BRANCH=master #default to master
	fi
	echo "Cloning openjpeg-data from ${OPJ_DATA_BRANCH} branch"
	git clone -v --depth=1 --branch=${OPJ_DATA_BRANCH} https://github.com/uclouvain/openjpeg-data data

	# We need jpylyzer for the test suite
    JPYLYZER_VERSION="1.17.0"    
	echo "Retrieving jpylyzer"
	if [ "${TRAVIS_OS_NAME:-}" == "osx"  -o "${RUNNER_OS:-}" == "macOS" ] || uname -s | grep -i Darwin &> /dev/null; then
        echo "Skip Retrieving jpylyzer on OSX. Related tests no longer work on CI"
	elif [ "${APPVEYOR:-}" == "True" -o "${RUNNER_OS:-}" == "Windows" ]; then
		wget -q https://github.com/openpreserve/jpylyzer/releases/download/${JPYLYZER_VERSION}/jpylyzer_${JPYLYZER_VERSION}_win32.zip
		mkdir jpylyzer
		cd jpylyzer
		cmake -E tar -xf ../jpylyzer_${JPYLYZER_VERSION}_win32.zip
		cd ..
	else
		wget -qO - https://github.com/openpreserve/jpylyzer/archive/${JPYLYZER_VERSION}.tar.gz | tar -xz
		mv jpylyzer-${JPYLYZER_VERSION}/jpylyzer ./
		chmod +x jpylyzer/jpylyzer.py
	fi

	# When OPJ_NONCOMMERCIAL=1, kakadu trial binaries are used for testing. Here's the copyright notice from kakadu:
	# Copyright is owned by NewSouth Innovations Pty Limited, commercial arm of the UNSW Australia in Sydney.
	# You are free to trial these executables and even to re-distribute them, 
	# so long as such use or re-distribution is accompanied with this copyright notice and is not for commercial gain.
	# Note: Binaries can only be used for non-commercial purposes.
	if [ "${OPJ_NONCOMMERCIAL:-}" == "1" ]; then
		if [ "${TRAVIS_OS_NAME:-}" == "linux" -o "${RUNNER_OS:-}" == "Linux" ] || uname -s | grep -i Linux &> /dev/null; then
			echo "Retrieving Kakadu"
			wget -q http://kakadusoftware.com/wp-content/uploads/KDU841_Demo_Apps_for_Linux-x86-64_231117.zip
			cmake -E tar -xf KDU841_Demo_Apps_for_Linux-x86-64_231117.zip
			mv KDU841_Demo_Apps_for_Linux-x86-64_231117 kdu
		elif [ "${TRAVIS_OS_NAME:-}" == "osx"  -o "${RUNNER_OS:-}" == "macOS" ] || uname -s | grep -i Darwin &> /dev/null; then
			echo "Retrieving Kakadu"
			wget -v http://kakadusoftware.com/wp-content/uploads/KDU841_Demo_Apps_for_MacOS_231117.dmg_.zip
			cmake -E tar -xf KDU841_Demo_Apps_for_MacOS_231117.dmg_.zip
			wget -q http://downloads.sourceforge.net/project/catacombae/HFSExplorer/0.23/hfsexplorer-0.23-bin.zip
			mkdir hfsexplorer && cmake -E chdir hfsexplorer tar -xf ../hfsexplorer-0.23-bin.zip
			./hfsexplorer/bin/unhfs.sh -o ./ -fsroot Kakadu-demo-apps.pkg  KDU841_Demo_Apps_for_MacOS_231117.dmg
			pkgutil --expand Kakadu-demo-apps.pkg ./kdu
			cd kdu
			cat libkduv84r.pkg/Payload | gzip -d | cpio -id
			cat kduexpand.pkg/Payload | gzip -d | cpio -id
			cat kducompress.pkg/Payload | gzip -d | cpio -id
			install_name_tool -id ${PWD}/libkdu_v84R.dylib libkdu_v84R.dylib 
			install_name_tool -change /usr/local/lib/libkdu_v84R.dylib ${PWD}/libkdu_v84R.dylib kdu_compress
			install_name_tool -change /usr/local/lib/libkdu_v84R.dylib ${PWD}/libkdu_v84R.dylib kdu_expand
		elif [ "${APPVEYOR:-}" == "True" -o "${RUNNER_OS:-}" == "Windows"  ] || uname -s | grep -i MINGW &> /dev/null || uname -s | grep -i CYGWIN &> /dev/null; then
			echo "Retrieving Kakadu"
			wget -q http://kakadusoftware.com/wp-content/uploads/KDU841_Demo_Apps_for_Win64_231117.msi_.zip
			cmake -E tar -xf KDU841_Demo_Apps_for_Win64_231117.msi_.zip
			msiexec /i KDU841_Demo_Apps_for_Win64_231117.msi /quiet /qn /norestart
			if [ -d "C:/Program Files/Kakadu" ]; then
				cp -r "C:/Program Files/Kakadu" ./kdu
			else
				cp -r "C:/Program Files (x86)/Kakadu" ./kdu
			fi
		fi
	fi
fi

if [ "${OPJ_CI_CHECK_STYLE:-}" == "1" ]; then
    pip install --user autopep8
fi
