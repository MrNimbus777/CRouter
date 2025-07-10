# CRouter – A High-Performance C++ Web Framework

**CRouter** is a lightweight, high-performance backend web framework written in modern **C++**, built atop **Boost.Asio** and related Boost libraries. Designed for speed, extensibility, and simplicity, it enables clean development of modular web servers and APIs.

---

## Features

* **Parallel HTTP Request Handling**
  Built on Boost.Asio for true asynchronous and concurrent request processing.

* **Background Worker Pool**
  Heavy or blocking tasks are seamlessly offloaded to a background thread pool.

* **Auto Routing System**
  Simple and clean routing based on URL paths, automatically delegating to plugins.

* **Powerful Plugin System**
  Write modular handlers as standalone `.cpp` files that compile and integrate dynamically. Customizable and ideal for extensions.

* **Static File Server**
  Built-in support for serving static files, using [MimeTypes](https://github.com/lasselukkari/MimeTypes) by **lasselukkari** for proper content-type resolution.

---

## Build Dependencies

Make sure the following libraries are available before building:

* [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
* [Boost.System](https://www.boost.org/doc/libs/release/libs/system/)
* [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/)
* [RapidJSON](https://github.com/Tencent/rapidjson)

---

## Getting Started

### Option 1: Use Prebuilt Binaries

1. Download one of the prebuilt versions:

   * [Windows](https://github.com/MrNimbus777/CRouter/releases/tag/windows-prebuilt)
   * [Linux](https://github.com/MrNimbus777/CRouter/releases/tag/linux-prebuilt)

2. Run the executable. On first launch, it will:

   * Start a basic web server
   * Generate a sample **Hello World** page
   * Create an example plugin file: `/app/handlers/test.cpp`

> **Note**: You must have `g++` installed (**MinGW** recommended on Windows). The plugin system strictly depends on the `g++` command to compile handlers. Using other compilers (like MSVC) may cause the plugin system to fail.

---

### Option 2: Build from Source

Clone the repository and compile it using your favorite environment. Ensure all dependencies are installed, and that you're using **g++** as your compiler.

```bash
git clone https://github.com/yourname/CRouter.git
cd CRouter
mkdir build && cd build
cmake ..
make
```

---

## How It Works

### Plugin-based Routing

CRouter's routing system maps incoming request paths directly to handler plugins:

* Example URL:
  `https://example.com/some_route`

* This will trigger the plugin:
  `/app/handlers/some_route.cpp`

Each plugin defines the logic for that route. On each request, CRouter dynamically loads and executes the plugin to handle it.

---

### Custom Default Request Handler

You can override the default fallback handler:

1. Update the `config.json` with the appropriate setting.
2. On the next server start, CRouter will auto-generate:
   `/app/custom_default_request_handler.cpp`

You can edit this file to customize how unhandled requests are processed.

---

## Credits

* **MimeTypes** [https://github.com/lasselukkari/MimeTypes](https://github.com/lasselukkari/MimeTypes)
* **Boost** [https://www.boost.org/](https://www.boost.org/)

---

Made with ❤️ in C++