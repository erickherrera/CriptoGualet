# CriptoGualet
CriptoGualet is a **cross-platform cryptocurrency wallet** designed for secure and easy management of your digital assets. It's built with a modern, intuitive user interface using the **Qt Framework** and is configured for cross-platform compilation using **CMake**.

---

## Features

* **Secure Transactions**: Utilizes industry-standard cryptographic libraries for secure and reliable transactions.
* **Cross-Platform Support**: Built with CMake to ensure compatibility across various operating systems, including Windows, macOS, and Linux.
* **Modern UI**: A clean, intuitive, and user-friendly interface powered by the Qt Framework.
* **Multiple Cryptocurrency Support**: Bitcoin, Ethereum, Litecoin, etc.

---

## Prerequisites

To build CriptoGualet, you'll need the following:

* **CMake**: A cross-platform build system. You can download it from [cmake.org](https://cmake.org/).
* **Qt Framework**: The complete Qt development environment. You can get it from [qt.io](https://www.qt.io/).
* A C++ compiler that supports C++11 or later (e.g., GCC, Clang, MSVC).

---

## Building the Project

### 1. Clone the Repository

```bash
git clone https://github.com/erickherrera/CriptoGualet.git
cd CriptoGualet
````

### 2\. Configure with CMake

Create a `build` directory and run CMake to configure the project.

```bash
mkdir build
cd build
cmake ..
```

### 3\. Build the Application

Use the generated build files to compile the application.

```bash
cmake --build .
```

After a successful build, the executable will be located in the `build` directory.

-----

## Usage

CriptoGualet provides a graphical interface for managing your cryptocurrency wallets. After building the application, you can launch it from the build directory or install it to your system.

```bash
# Launch from build directory (Linux/macOS)
./build/CriptoGualet

# Launch from build directory (Windows)
.\build\CriptoGualet.exe
```

On first launch, you'll be guided through creating a new wallet or importing an existing one. Make sure to securely back up your recovery phrase.

-----

## Contributing

We welcome contributions\! Please feel free to fork the repository, make changes, and submit a pull request.

-----

## License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
