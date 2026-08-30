# engine

## To use

## Install

depends on
- vulkan
- sdl (pull vendor submodule)

```sh
cmake -B build
cmake --build build
```

## Compiling Shaders
```sh
slangc src/shaders/shader.slang -target spirv -profile spirv_1_3 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o slang.spv
```

## Usage

```sh
./build/engine.exe
```

## Contributing

PRs maybe.

## License

MIT © Max Inhat et al.
