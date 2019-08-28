sled
======

It works! sled it!

Define the FIRMWARE_HOME variable using `export FIRMWARE_HOME=../path/to/card10/firmware/root`

Update libsled.a, copy this folder to `${FIRMWARE_HOME}/l0dables/sled` and add sled as a build target to card10 by TYPING the following commands in the main sled folder.
```sh
cp Makefiles/sledconf.card10 sledconf &&
make clean libsled.a &&
mv libsled.a src/ext/card10/sled/lib/libsled.a &&
if [ -z ${FIRMWARE_HOME+x} ]
then
	echo "FIRMWARE_HOME needs to be defined"
else
	rm -r ${FIRMWARE_HOME}/l0dables/sled &&
	cp -r src/ext/card10/sled ${FIRMWARE_HOME}/l0dables/sled &&
fi &&
grep -qxF "subdir('sled/')" ${FIRMWARE_HOME}/l0dables/meson.build || (echo "subdir('sled/')" >> ${FIRMWARE_HOME}/l0dables/meson.build)
```

Bootstrap/compile card10
```sh
./bootstrap.sh -Djailbreak_card10=true &&
ninja -C build
```
