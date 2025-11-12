
Installation:
Installing the library is a relatively simple operation which involves copying files. Use the following steps to install. (Assuming you have copied the .dmg package into ~/Downloads/D3XX/)

1. Open the terminal window (Finder -> Go -> Utilities -> Terminal).
2. If the /usr/local/lib directory does not exist, create it:
    sudo mkdir -p /usr/local/lib

3. If the /usr/local/include directory does not exist, create it:
    sudo mkdir -p /usr/local/include
    sudo cp ftd3xx.h  /usr/local/include
    sudo cp Types.h  /usr/local/include

4. Copy the dylib files to /usr/local/lib:
    sudo cp ~/Downloads/D3XX/libftd3xx.dylib /usr/local/lib/
    sudo cp ~/Downloads/D3XX/libftd3xx.FULL_VERSION_NUMBER.dylib /usr/local/lib/

5. Add the library path to your bash profile:
    echo 'export DYLD_LIBRARY_PATH=/usr/local/lib' >> ~/.bash_profile
    source ~/.bash_profile

6. Build:
    make

7. Run the sample application:

    ./rw 1024 1024

   or with specified library path:

    DYLD_LIBRARY_PATH=. ./rw 1024 1024
