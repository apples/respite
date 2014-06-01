Respite
===

Introduction
---

Respite is a lightweight C++ project builder.

Features
---

- Builds C++ projects.
- Complete header dependency checking.
- No configuration.

Compiling
---

There's a single source file and a one-line `build.sh` script.
I'm sure you can figure it out.

### Dependencies

- Boost::Filesystem
- libexecstream

Usage
---

Set up your project directory so that all `.cpp` files are located in `src/`.

Run Respite from within your project directory:

```sh
$ respite
```

This will produce an executable called `a.respite`.

Configuration
---

Respite has no configuration.

Environment
---

The following environment variables affect Respite:

- `CXX`
- `CXXFLAGS`
- `LDFLAGS`
- `LDLIBS`

Respite builds `.cpp` files into `.o` files with this command:

```sh
$ $CXX $CXXFLAGS -c file.cpp -o file.o
```

All built `.o` files are linked into `a.respite` with this command:

```sh
$ $CXX $LDFLAGS *.o $LDLIBS -o a.respite
```

Project Directory
---

Respite assumes your project directory looks like this:

- `src/` - source code

Respite will create additional files and directories:

- `.respite/` - cache
- `a.respite` - executable

Other files and directories are free to exist in the project directory;
Respite will not touch them.

FAQ
---

### How do I configure Respite?

Respite has no configuration.
It simply builds the project.

### How do I create multiple targets with respite?

Create a simple helper script that manages symlinks to the cache directory:

```sh
#!/bin/sh
TARGET=$1

case $TARGET in
    release)
        export CXXFLAGS="-O3"
        ;;
    debug)
        export CXXFLAGS="-g"
        ;;
    *)
        echo "Nope."
        exit 1
        ;;
esac

mkdir -p .respite-$TARGET
ln -s .respite-$TARGET .respite
respite
mv a.respite app-$TARGET
rm .respite
```

### How do I clean or force a rebuild?

Simply delete the build cache:

```sh
$ rm -rf .respite
```

Disclaimer
---

Respite might eat your soul. Warranty always void.

License
---

(zlib/libpng)

Copyright (c) 2014 Jeramy Harrison

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
