# ! /bin/bash 
# $Id: new_build.sh 1705 2010-09-23 11:46:10Z bradbell $
# -----------------------------------------------------------------------------
# CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-10 Bradley M. Bell
#
# CppAD is distributed under multiple licenses. This distribution is under
# the terms of the 
#                     Common Public License Version 1.0.
#
# A copy of this license is included in the COPYING file of this distribution.
# Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
# -----------------------------------------------------------------------------
# 1. Change ./new_build.sh doxygen to put output in doxygen.log
# -----------------------------------------------------------------------------
# prefix directories for the corresponding packages
CPPAD_DIR=$HOME/prefix/cppad  
BOOST_DIR=/usr/include
ADOLC_DIR=$HOME/prefix/adolc
FADBAD_DIR=$HOME/prefix/fadbad
SACADO_DIR=$HOME/prefix/sacado
IPOPT_DIR=$HOME/prefix/ipopt
#
# library path for the ipopt and adolc
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$ADOLC_DIR/lib:$IPOPT_DIR/lib"
# -----------------------------------------------------------------------------
# exit on error
set -e
# -----------------------------------------------------------------------------
# run multiple options in order
if [ "$2" != "" ]
then
     for option in $*
     do
		echo "=============================================================="
		echo "begin: new_build.sh $option"
          ./new_build.sh $option
		echo "end: new_build.sh $option"
     done
     exit 0
fi
# -----------------------------------------------------------------------------
if [ ! -e work ]
then
	echo "new_build.sh: mkdir work"
	mkdir work
fi
# -----------------------------------------------------------------------------
# Today's date in yyyy-mm-dd decimal digit format where 
# yy is year in century, mm is month in year, dd is day in month.
yyyy_mm_dd=`date +%F`
#
# Version of cppad that corresponds to today.
version=`echo $yyyy_mm_dd | sed -e 's|-||g'`
#
# temporary source directory files that are created by the configure command 
configure_file_list="
	cppad/config.h
	cppad/configure.hpp
	doxyfile
	doc.omh
	omh/install_unix.omh
	omh/install_windows.omh
"
# -----------------------------------------------------------------------------
# change version to current date
if [ "$1" = "version" ]
then
	#
	# automatically change version for certain files
	# (the [.0-9]* is for using new_build.sh in CppAD/stable/* directories)
	#
	# libtool does not seem to support version by date
	# sed < cppad_ipopt/src/makefile.am > cppad_ipopt/src/makefile.am.$$ \
	#	-e "s/\(-version-info\) *[0-9]\{8\}[.0-9]*/\1 $version/"
	#
	echo "sed -i.old AUTHORS ..."
	sed -i.old AUTHORS \
		-e "s/, [0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\} *,/, $yyyy_mm_dd,/"
	#
	echo "sed -i.old configure.ac ..."
	sed -i.old configure.ac \
		-e "s/(CppAD, [0-9]\{8\}[.0-9]* *,/(CppAD, $version,/" 
	#
	echo "sed -i.old configure ..."
	sed -i.old configure \
		-e "s/CppAD [0-9]\{8\}[.0-9]*/CppAD $version/g" \
		-e "s/VERSION='[0-9]\{8\}[.0-9]*'/VERSION='$version'/g" \
		-e "s/configure [0-9]\{8\}[.0-9]*/configure $version/g" \
		-e "s/config.status [0-9]\{8\}[.0-9]*/config.status $version/g" \
		-e "s/\$as_me [0-9]\{8\}[.0-9]*/\$as_me $version/g" \
        	-e "s/Generated by GNU Autoconf.*$version/&./"
	#
	echo "sed -i.old cppad/config.h ..."
	sed -i.old cppad/config.h \
		-e "s/CppAD [0-9]\{8\}[.0-9]*/CppAD $version/g" \
		-e "s/VERSION \"[0-9]\{8\}[.0-9]*\"/VERSION \"$version\"/g"
	#
	list="
		AUTHORS
		configure.ac
		configure
		cppad/config.h
	"
	for name in $list
	do
		echo "-------------------------------------------------------------"
		echo "new_build.sh: diff $name.old $name"
		if diff $name.old $name
		then
			echo "	no difference was found"
		fi
		#
		echo "new_build.sh: rm $name.old"
		rm $name.old
	done
	echo "-------------------------------------------------------------"
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "automake" ] 
then
	#
	# check that autoconf and automake output are in original version
	makefile_in=`sed configure.ac \
        	-n \
        	-e '/END AC_CONFIG_FILES/,$d' \
        	-e '1,/AC_CONFIG_FILES/d' \
        	-e 's|/makefile$|&.in|' \
        	-e '/\/makefile.in/p'`
	auto_output="
		depcomp 
		install-sh 
		missing 
		configure 
		cppad/config.h 
		cppad/config.h.in 
		$makefile_in
	"
	missing=""
	for name in $auto_output
	do
		if [ ! -e $name ]
		then
			if [ "$missing" != "" ]
			then
				missing="$missing, $name"
			else
				missing="$name"
			fi
		fi
	done
	if [ "$missing" != "" ]
	then
		echo "new_build.sh: The following files:"
		echo "	$missing"
		echo "are not in subversion repository."
		echo "Check them in when this command is done completes."
	fi
	#
	echo "aclocal"
	aclocal
	#
	echo "autoheader"
	autoheader
	#
	echo "skipping libtoolize"
	# echo "libtoolize -c -f -i"
	# if ! libtoolize -c -f -i
	# then
	# 	exit 1
	# fi
	#
	echo "autoconf"
	autoconf
	#
	echo "automake --add-missing"
	automake --add-missing
	#
	link_list="missing install-sh depcomp"
	for name in $link_list
	do
		if [ -h "$name" ]
		then
			echo "Converting $name from a link to a regular file"
			#
			echo "cp $name $name.$$"
			cp $name $name.$$
			#
			echo "mv $name.$$ $name"
			mv $name.$$ $name
		fi
	done
	#
	exit 0
fi
# -----------------------------------------------------------------------------
# configure
if [ "$1" == "configure" ]
then
	echo "cd work"
	cd work
	#
	dir_list="
		--prefix=$CPPAD_DIR
		POSTFIX_DIR=coin
	"
	if [ -e $BOOST_DIR/boost ]
	then
		dir_list="$dir_list 
			BOOST_DIR=$BOOST_DIR"
	fi
	if [ -e $ADOLC_DIR/include/adolc ]
	then
		dir_list="$dir_list 
			ADOLC_DIR=$ADOLC_DIR"
	fi
	if [ -e $FADBAD_DIR/FADBAD++ ]
	then
		dir_list="$dir_list 
			FADBAD_DIR=$FADBAD_DIR"
	fi
	if [ -e $SACADO_DIR/include/Sacado.hpp ]
	then
		dir_list="$dir_list 
			SACADO_DIR=$SACADO_DIR"
	fi
	if [ -e $IPOPT_DIR/include/coin/IpIpoptApplication.hpp ]
	then
		dir_list="$dir_list 
		IPOPT_DIR=$IPOPT_DIR"
	fi
	dir_list=`echo $dir_list | sed -e 's|\t\t*| |g'`
	echo "../configure \\"
	echo "$dir_list" | sed -e 's| | \\\n\t|g' -e 's|$| \\|' -e 's|^|\t|'
	echo "	CXX_FLAGS=\"-Wall -ansi -pedantic-errors -std=c++98\"\\"
	echo "	--with-Documentation"
	#
	../configure $dir_list \
		CXX_FLAGS="-Wall -ansi -pedantic-errors -std=c++98" \
		--with-Documentation
	#
	# Fix makefile for what appears to be a bug in gzip under cygwin
	echo "../fix_makefile.sh"
	../fix_makefile.sh
	#
	# make shell scripts created by configure executable
	echo "chmod +x example/test_one.sh"
	chmod +x example/test_one.sh
	#
	echo "chmod +x test_more/test_one.sh"
	chmod +x test_more/test_one.sh
	#
	for file in $configure_file_list
	do
		echo "cp $file ../$file"
		cp $file ../$file
	done
	#
	exit 0
fi
#
# -----------------------------------------------------------------------------
if [ "$1" = "dist" ] 
then
	echo "cd work"
	cd work
	#
	if [ -e cppad-$version ]
	then
		echo "rm -r cppad-$version"
		rm -r cppad-$version
	fi
	for file in cppad-*.tgz 
	do
		if [ -e $file ]
		then
			echo "rm $file"
			rm $file
		fi
	done
	#
	echo "Only build the *.xml version of the documentation for distribution"
	if ! grep < doc.omh > /dev/null \
		'This comment is used to remove the table below' 
	then
		echo "new_build.sh: Missing comment expected in doc.omh"
		echo "Try re-running new_build.sh configure to generate it."
		exit 1
	fi
	echo "sed -i.save doc.omh ..."
	sed -i.save doc.omh \
		-e '/This comment is used to remove the table below/,/$tend/d'
	#
	if [ -e doc ]
	then
		echo "rm -r doc"
		rm -r doc
	fi
	#
	echo "mkdir doc"
	mkdir doc
	#
	echo "cd doc"
	cd doc
	#
	log_file="../../omhelp.doc.xml"
	home_page="http://www.coin-or.org/CppAD/"
	echo "omhelp ../../doc.omh -noframe -debug -l $home_page -xml \\"
	echo "	> $log_file"
	if ! omhelp ../../doc.omh -noframe -debug -l $home_page -xml > $log_file
	then
		grep "^OMhelp Error:" $log_file
		exit 1
	fi
	#
	if grep "^OMhelp Warning:" $log_file
	then
		exit 1
	fi
	#
	echo "cd .."
	cd ..
	#
	echo "mv doc.omh.save doc.omh"
	mv doc.omh.save doc.omh
	#
	echo "make dist"
	make dist
	#
	if [ ! -e cppad-$version.tar.gz ]
	then
		echo "cppad-$version.tar.gz does not exist"
		echo "perhaps version is out of date"
		#
		exit 1
	fi
	# change *.tgz to *.cpl.tgz
	echo "mv cppad-$version.tar.gz cppad-$version.cpl.tgz"
	mv cppad-$version.tar.gz cppad-$version.cpl.tgz
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "omhelp" ] 
then
	if ! grep < doc.omh > /dev/null \
		'This comment is used to remove the table below'
	then
		echo "new_build.sh: doc.omh is missing a table."
		echo "Try re-running new_build.sh configure."
	fi
	for flag in "printable" ""
	do
		for ext in htm xml
		do
			echo "begin: ./run_omhelp.sh doc $ext $flag"
			./run_omhelp.sh doc $ext $flag
			echo="end: ./run_omhelp.sh doc $ext $flag"
		done
	done
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "doxygen" ]
then
	if [ -e doxygen.err ]
	then
		echo "rm doxygen.err"
		rm doxygen.err
	fi
	#
	if [ -e doxydoc ]
	then
		echo "rm -r doxydoc"
		rm -r doxydoc
	fi
	#
	echo "mkdir doxydoc"
	mkdir doxydoc
	#
	echo "doxygen doxyfile"
	doxygen doxyfile
	#
	echo "cat doxygen.err"
	cat doxygen.err 
	#
	echo "./check_doxygen.sh"
	./check_doxygen.sh
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "test" ] 
then
	log_file="build_test.log"
	#
	# start log for this test
	echo "date > $log_file"
	date       > $log_file
	#
	echo "./check_include_omh.sh >> $log_file"
	./check_include_omh.sh       >> $log_file
	# -------------------------------------------------------------
	# Run automated checking of file names in original source directory
	#
	list="
		check_example.sh
		check_include_def.sh
		check_include_file.sh
		check_makefile.sh
		check_if_0.sh
	"
	for check in $list 
	do
		echo "./$check >> $log_file"
		./$check       >> $log_file
	done
	# add a new line after last file check
	echo ""             >> $log_file
	#
	# -----------------------------------------------------------------------
	echo "cd work"
	cd work
	log_file="../$log_file"
	#
	# erase old distribution directory
	if [ -e cppad-$version ]
	then
		echo "rm -r cppad-$version"
		rm -r cppad-$version
	fi
	#
	# create distribution directory
	echo "tar -xzf cppad-$version.cpl.tgz"
	tar -xzf cppad-$version.cpl.tgz
	#
	# -----------------------------------------------------------------------
	echo "cd cppad-$version"
	cd cppad-$version
	log_file="../$log_file"
	#
	echo "./new_build.sh configure >> $log_file"
	./new_build.sh configure       >> $log_file
	#
	# test user documentation
	echo "./run_omhelp.sh doc xml  >> $log_file"
	./run_omhelp.sh doc xml        >> $log_file
	# 
	# test developer documentation
	echo "./new_build.sh doxygen   >> $log_file"
	./new_build.sh doxygen         >> $log_file
	#
	# -----------------------------------------------------------------------
	echo "cd work"
	cd work
	log_file="../$log_file"
	#
	dir=`pwd` 
	echo "Use: tail -f $dir/make_test.log"
	echo "to follow the progress of the following command:"
	#
	# build and run all the tests
	echo "make test                >& make_test.log"
	make test                      >& make_test.log
	#
	echo "cat make_test.log        >> $log_file"                            
	cat make_test.log              >> $log_file                            
	#
	if grep 'warning:' make_test.log
	then
		echo "There are warnings in $dir/make.log"
		exit 1
	fi
	#
	echo "cat test.log             >> $log_file"
	cat test.log                   >> $log_file
	# --------------------------------------------------------------------
	echo "cd ../../.."
	cd ../../..
	# end the build_test.log file with the date and time
	date >> build_test.log
	#
	dir=`pwd`
	echo "Check $dir/build_test.log for errors and warnings."
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" == "openmp" ]
then
	echo "openmp/run.sh  | tee openmp.log"
	openmp/run.sh        | tee openmp.log
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "gpl" ] 
then
	# create GPL licensed version
	echo "./gpl_license.sh"
	./gpl_license.sh
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" = "doc" ] 
then
	if [ -e doc ]
	then
		echo "rm -r doc"
		rm -r doc
	fi
	echo "cp -r work/doc doc"
	cp -r work/doc doc
	#
	for ext in cpl gpl
	do
		echo "cp work/cppad-$version.cpl.tgz doc/cppad-$version.cpl.tgz"
		cp work/cppad-$version.cpl.tgz doc/cppad-$version.cpl.tgz
	done
	echo "cp -r doxydoc doc/doxydoc"
	cp -r doxydoc doc/doxydoc
	#
	exit 0
fi
# -----------------------------------------------------------------------------
if [ "$1" == "all" ]
then
	list="
		version
		automake
		configure
		dist
		omhelp
		doxygen
		test
		openmp
		gpl
		doc
	"
	echo "./new_build.sh $list"
	./new_build.sh $list
fi
# -----------------------------------------------------------------------------
# report new_build.sh usage error
if [ "$1" != "" ]
then
     echo "$1 is not a valid option"
fi
#
cat << EOF
usage: new_build.sh option_1 option_2 ...

options
-------
version:   update version in AUTHORS, configure.ac, configure, config.h
automake:  run the tools required by autoconf and automake.
configure: run the configure script in the work directory.
dist:      create the distribution file work/cppad-version.cpl.tgz
omhelp:    build all formats of user documentation in doc/*
doxygen:   build developer documentation in doxydoc/*
test:      unpack work/*.cpl.tgz, run make test, put result in build_test.log
openmp:    run the openmp tests and put results in openmp.log
gpl:       create work/*.gpl.zip and work/*.cpl.zip       
doc:       create ./doc with tarballs and developer documentation

all:       run all the options above in order
EOF
#
exit 1