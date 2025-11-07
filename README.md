# libbndl

A library to read Bundle archives used in titles made by Criterion Games, such as Burnout Paradise, Need For Speed: Hot Pursuit (2010) and Need for Speed: Most Wanted (2012).

# How to build

```sh
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

# How to use the library

```c++
#include <libbndl/bundle.hpp>

int main(int argc, char **argv)
{
    // Create a bundle instance
    libbndl::Bundle bundle;

    // Load the archive
    if (!bundle.Load(argv[1]))
		return -1;

    // Load a resource from the archive
    const auto resource = bundle.GetResource(argv[2]);
	if (!resource)
		return -2;

	// Get the data from the resource binary that's assigned to main memory
	const auto buffer = resource->GetBinary(libbndl::MemoryType::MainMemory);

	// On the buffer you can use operator[], standard iterator functions or call GetData() to get the underlying pointer.
	// Consider using https://github.com/Bo98/libbinaryio to parse the underlying data structures in the resource.
}
```

You can also create new bundles or modify existing ones. See [bundle.hpp](include/libbndl/bundle.hpp) for more information.
