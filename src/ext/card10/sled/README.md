sled
======

It works! sled it!

Define the FIRMWARE_HOME variable using `export FIRMWARE_HOME=../path/to/card10/firmware/root`

Update libsled.a, copy this folder to `${FIRMWARE_HOME}/l0dables/sled` and add sled as a build target to card10 by TYPING the following commands in the main sled folder.
```sh
cp Makefiles/sledconf.card10 sledconf &&
make clean libsled.a &&
mkdir -p src/ext/card10/sled/lib &&
mv libsled.a src/ext/card10/sled/lib/libsled.a &&
if [ -z ${FIRMWARE_HOME+x} ]
then
	echo "FIRMWARE_HOME needs to be defined"
else
	if [ -d ${FIRMWARE_HOME}/l0dables/sled ]; then rm -r ${FIRMWARE_HOME}/l0dables/sled; fi &&
	cp -r src/ext/card10/sled ${FIRMWARE_HOME}/l0dables/sled &&
fi &&
(grep -qxF "subdir('sled/')" ${FIRMWARE_HOME}/l0dables/meson.build || (echo "subdir('sled/')" >> ${FIRMWARE_HOME}/l0dables/meson.build)) &&
echo "done."
```

Bootstrap/compile card10
```sh
## before 1.9
# ./bootstrap.sh -Djailbreak_card10=true && ninja -C build
## after 1.9
./bootstrap.sh && ninja -C build
```

Now copy `build/l0dable/sled/sled.elf` to `${card10_flash}/apps/sled.elf`
Maybe also update the firmware by copying `build/pycardium/pycardium_epicardium.bin`to `${card10_flash}/card10.bin`
After firmware version 1.9 you have to enbable elf loading by creating `echo "execute_elf=1" >> ${card10_flash}/card10.cfg`

