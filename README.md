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
git clone [https://github.com/your-username/CriptoGualet.git](https://github.com/your-username/CriptoGualet.git)
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

(Provide a brief description of how to run the application, e.g., double-click the executable or run from the command line.)

```bash
# Example for a Linux/macOS build
./build/CriptoGualet

# Example for a Windows build
.\build\CriptoGualet.exe
```

-----

## Contributing

We welcome contributions\! Please feel free to fork the repository, make changes, and submit a pull request.

-----

## License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
