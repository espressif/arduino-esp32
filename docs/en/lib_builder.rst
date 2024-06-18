###############
Library Builder
###############

About
-----

Espressif provides a `tool <https://github.com/espressif/esp32-arduino-lib-builder>`_ to simplify building your own compiled libraries for use in Arduino IDE (or your favorite IDE).

This tool can be used to change the project or a specific configuration according to your needs.

Installing
----------

To install the Library Builder into your environment, please, follow the instructions below.

- Clone the ESP32 Arduino lib builder:

.. code-block:: bash

    git clone https://github.com/espressif/esp32-arduino-lib-builder

- Go to the ``esp32-arduino-lib-builder`` folder:

.. code-block:: bash

    cd esp32-arduino-lib-builder

- Build:

.. code-block:: bash

    ./build.sh

If everything works, you may see the following message: ``Successfully created esp32 image.``

Dependencies
************

To build the library you will need to install some dependencies. Maybe you already have installed it, but it is a good idea to check before building.

- Install all dependencies (**Ubuntu**):

.. code-block:: bash

    sudo apt-get install git wget curl libssl-dev libncurses-dev flex bison gperf cmake ninja-build ccache jq

- Install Python and upgrade pip:

.. code-block:: bash

    sudo apt-get install python3
    sudo pip install --upgrade pip

- Install all required packages:

.. code-block:: bash

    pip install --user setuptools pyserial click cryptography future pyparsing pyelftools

Building
--------

If you have all the dependencies met, it is time to build the libraries.

To build using the default configuration:

.. code-block:: bash

    ./build.sh

Custom Build
************

There are some options to help you create custom libraries. You can use the following options:

Usage
^^^^^

.. code-block:: bash

    build.sh [-s] [-A arduino_branch] [-I idf_branch] [-i idf_commit] [-c path] [-t <target>] [-b <build|menuconfig|idf_libs|copy_bootloader|mem_variant>] [config ...]

Skip Install/Update
^^^^^^^^^^^^^^^^^^^

Skip installing/updating of ESP-IDF and all components

.. code-block:: bash

    ./build.sh -s

This option can be used if you already have the ESP-IDF and all components already in your environment.

Set Arduino-ESP32 Branch
^^^^^^^^^^^^^^^^^^^^^^^^

Set which branch of arduino-esp32 to be used for compilation

.. code-block:: bash

    ./build.sh -A <arduino_branch>

Set ESP-IDF Branch
^^^^^^^^^^^^^^^^^^

Set which branch of ESP-IDF is to be used for compilation

.. code-block:: bash

    ./build.sh -I <idf_branch>

Set the ESP-IDF Commit
^^^^^^^^^^^^^^^^^^^^^^

Set which commit of ESP-IDF to be used for compilation

.. code-block:: bash

    ./build.sh -i <idf_commit>

Deploy
^^^^^^

Deploy the build to github arduino-esp32

.. code-block:: bash

    ./build.sh -d

Set the Arduino-ESP32 Destination Folder
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set the arduino-esp32 folder to copy the result to. ex. '$HOME/Arduino/hardware/espressif/esp32'

.. code-block:: bash

    ./build.sh -c <path>

This function is used to copy the compiled libraries to the Arduino folder.

Set the Target
^^^^^^^^^^^^^^

Set the build target(chip). ex. 'esp32s3'

.. code-block:: bash

    ./build.sh -t <target>

This build command will build for the ESP32-S3 target. You can specify other targets.

* esp32
* esp32s2
* esp32s3
* esp32c2
* esp32c3
* esp32c6
* esp32h2

Set Build Type
^^^^^^^^^^^^^^

Set the build type. ex. 'build' to build the project and prepare for uploading to a board.

.. note:: This command depends on the ``-t`` argument.

.. code-block:: bash

    ./build.sh -t esp32 -b <build|menuconfig|idf_libs|copy_bootloader|mem_variant>

Additional Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

Specify additional configs to be applied. ex. 'qio 80m' to compile for QIO Flash@80MHz.

.. note:: This command requires the ``-b`` to work properly.


.. code-block:: bash

    ./build.sh -t esp32 -b idf_libs qio 80m

Docker Image
------------

You can use a docker image for building the static libraries of ESP-IDF components for use in Arduino projects.
This image contains a copy of the ``esp32-arduino-lib-builder`` repository and already includes or will obtain all the required tools and dependencies to build the Arduino static libraries.

The current supported architectures by the Docker image are:

* ``amd64``
* ``arm64``

.. note::
    Building the libraries using the Docker image is much slower than building them natively on the host machine.
    It is recommended to use the Docker image only when the host machine does not meet the requirements for building the libraries.

Tags
****

Multiple tags of this image are maintained:

- ``latest``: tracks ``master`` branch of the Lib Builder
- ``release-vX.Y``: tracks ``release/vX.Y`` branch of the Lib Builder

.. note::
    Versions of Lib Builder released before this feature was introduced do not have corresponding Docker image versions.
    You can check the up-to-date list of available tags at https://hub.docker.com/r/espressif/esp32-arduino-lib-builder/tags.

Usage
*****

Before using the ``espressif/esp32-arduino-lib-builder`` Docker image locally, make sure you have Docker installed.
Follow the instructions at https://docs.docker.com/install/, if it is not installed yet.

If using the image in a CI environment, consult the documentation of your CI service on how to specify the image used for the build process.

Building the Libraries
^^^^^^^^^^^^^^^^^^^^^^

You have two options to run the Docker image to build the libraries. Manually or using the provided run script.

To run the Docker image manually, use the following command from the root of the ``arduino-esp32`` repository:

.. code-block:: bash

    docker run --rm -it -v $PWD:/arduino-esp32 -e TERM=xterm-256color espressif/esp32-arduino-lib-builder

This will start the Lib Builder UI for compiling the libraries. The above command explained:

- ``docker run``: Runs a command in a container;
- ``--rm``: Optional. Automatically removes the container when it exits. Remove this flag if you plan to use the container multiple times;
- ``-i`` Run the container in interactive mode;
- ``-t`` Allocate a pseudo-TTY;
- ``-e TERM=xterm-256color``: Optional. Sets the terminal type to ``xterm-256color`` to display colors correctly;
- ``-v $PWD:/arduino-esp32``: Optional. Mounts the current folder at ``/arduino-esp32`` inside the container. If not provided, the container will not copy the compiled libraries to the host machine;
- ``espressif/esp32-arduino-lib-builder``: uses Docker image ``espressif/esp32-arduino-lib-builder`` with tag ``latest``. The ``latest`` tag is implicitly added by Docker when no tag is specified.

.. warning::
    The ``-v`` option is used to mount a folder from the host machine to the container. Make sure the folder already exists on the host machine before running the command.
    Otherwise, the folder will be created with root permissions and files generated inside the container might cause permission issues and compilation errors.

.. note::
   When the mounted directory ``/arduino-esp32`` contains a git repository owned by a different user (``UID``) than the one running the Docker container,
   git commands executed within ``/arduino-esp32`` might fail, displaying an error message ``fatal: detected dubious ownership in repository at '/arduino-esp32'``.
   To resolve this issue, you can designate the ``/arduino-esp32`` directory as safe by setting the ``LIBBUILDER_GIT_SAFE_DIR`` environment variable during the Docker container startup.
   For instance, you can achieve this by including ``-e LIBBUILDER_GIT_SAFE_DIR='/arduino-esp32'`` as a parameter. Additionally, multiple directories can be specified by using a ``:`` separator.
   To entirely disable this git security check, ``*`` can be used.

After running the above command, you will be inside the container and the libraries can be built using the user interface.

By default the docker container will run the user interface script. If you want to run a specific command, you can pass it as an argument to the ``docker run`` command.
For example, to run a terminal inside the container, you can run:

.. code-block:: bash

    docker run -it espressif/esp32-arduino-lib-builder:latest /bin/bash

Running the Docker image using the provided run script will depend on the host OS.
Use the following command from the root of the ``arduino-esp32`` repository to execute the image in a Linux or macOS environment:

.. code-block:: bash

    curl -LJO https://raw.githubusercontent.com/espressif/esp32-arduino-lib-builder/master/tools/docker/run.sh
    chmod +x run.sh
    ./run.sh $PWD

For Windows, use the following command in PowerShell from the root of the ``arduino-esp32`` repository:

.. code-block:: powershell

    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/espressif/esp32-arduino-lib-builder/master/tools/docker/run.ps1" -OutFile "run.ps1"
    .\run.ps1 $pwd

.. warning::
    It is always a good practice to understand what the script does before running it.
    Make sure to analyze the content of the script to ensure it is safe to run and won't cause any harm to your system.

Building Custom Images
**********************

To build a custom Docker image, you need to clone the Lib Builder repository and use the provided Dockerfile in the Lib Builder repository. The Dockerfile is located in the ``tools/docker`` directory.

The `Docker file in the Lib Builder repository <https://github.com/espressif/esp32-arduino-lib-builder/blob/master/tools/docker/Dockerfile>`_ provides several build arguments which can be used to customize the Docker image:

- ``LIBBUILDER_CLONE_URL``: URL of the repository to clone Lib Builder from. Can be set to a custom URL when working with a fork of Lib Builder. The default is ``https://github.com/espressif/esp32-arduino-lib-builder.git``;
- ``LIBBUILDER_CLONE_BRANCH_OR_TAG``: Name of a git branch or tag used when cloning Lib Builder. This value is passed to the ``git clone`` command using the ``--branch`` argument. The default is ``master``;
- ``LIBBUILDER_CHECKOUT_REF``: If this argument is set to a non-empty value, ``git checkout $LIBBUILDER_CHECKOUT_REF`` command performs after cloning. This argument can be set to the SHA of the specific commit to check out, for example, if some specific commit on a release branch is desired;
- ``LIBBUILDER_CLONE_SHALLOW``: If this argument is set to a non-empty value, ``--depth=1 --shallow-submodules`` arguments are used when performing ``git clone``. Depth can be customized using ``LIBBUILDER_CLONE_SHALLOW_DEPTH``. Doing a shallow clone significantly reduces the amount of data downloaded and the size of the resulting Docker image. However, if switching to a different branch in such a "shallow" repository is necessary, an additional ``git fetch origin <branch>`` command must be executed first;
- ``LIBBUILDER_CLONE_SHALLOW_DEPTH``: This argument specifies the depth value to use when doing a shallow clone. If not set, ``--depth=1`` will be used. This argument has effect only if ``LIBBUILDER_CLONE_SHALLOW`` is used. Use this argument if you are building a Docker image for a branch, and the image has to contain the latest tag on that branch. To determine the required depth, run ``git describe`` for the given branch and note the offset number. Increment it by 1, then use it as the value of this argument. The resulting image will contain the latest tag on the branch, and consequently ``git describe`` command inside the Docker image will work as expected;

To use these arguments, pass them via the ``--build-arg`` command line option. For example, the following command builds a Docker image with a shallow clone of Lib Builder from a specific repository and branch:

.. code-block:: bash

    docker buildx build -t lib-builder-custom:master \
        --build-arg LIBBUILDER_CLONE_BRANCH_OR_TAG=master \
        --build-arg LIBBUILDER_CLONE_SHALLOW=1 \
        --build-arg LIBBUILDER_CLONE_URL=https://github.com/espressif/esp32-arduino-lib-builder \
        tools/docker
