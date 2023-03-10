#!/bin/bash

DEPLOT_DIR='RSU_Controller'
BUILD='build'
EXEC_DIR='exec'
APP_DIR='application'
TARGET='middleware'

rm -rf $DEPLOT_DIR
mkdir -p $DEPLOT_DIR/
mkdir -p $DEPLOT_DIR/$BUILD
mkdir -p $DEPLOT_DIR/$BUILD/$EXEC_DIR
mkdir -p $DEPLOT_DIR/$APP_DIR
cp $BUILD/$EXEC_DIR/$TARGET $DEPLOT_DIR/$BUILD/$EXEC_DIR/
cp -r j2735lib/ $DEPLOT_DIR/
cp -r config/ $DEPLOT_DIR/
rm -rf $DEPLOT_DIR/config/*_example

array=(`ls $APP_DIR | tr ',' ' '` )
for var in ${array[@]}
do
    if [ -d $APP_DIR/$var/'config' ]; then
        mkdir -p $DEPLOT_DIR/$APP_DIR/$var/
        cp -r $APP_DIR/$var/'config' $DEPLOT_DIR/$APP_DIR/$var/
        if [ -f $APP_DIR/$var/'config/config_example' ]; then
            rm -rf $DEPLOT_DIR/$APP_DIR/$var/config/*_example
        fi
    fi
done

mkdir -p $DEPLOT_DIR/log/

cp start.sh $DEPLOT_DIR/
chmod +x $DEPLOT_DIR/start.sh

cp script/install.sh $DEPLOT_DIR/
chmod +x $DEPLOT_DIR/install.sh

cp script/log_usage_check.sh $DEPLOT_DIR/
chmod +x $DEPLOT_DIR/log_usage_check.sh

cp script/ping_test.sh $DEPLOT_DIR/
chmod +x $DEPLOT_DIR/ping_test.sh