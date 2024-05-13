#include <Fwog/detail/ShaderCPP.h>
#include <Fwog/detail/ShaderGLSL.h>
#include <Fwog/Exception.h>

#include <string>
#include <utility>
#include <filesystem>
#include <fstream>

#if FWOG_VCC_ENABLE
extern "C"
{
  namespace shady
  {
  #include <shady/driver.h>
    extern void set_log_level(int);
  } // namespace shady
}
#endif

  #include <iostream>
namespace Fwog::detail
{
  namespace
  {
    // Helper for dealing with unsafe APIs
    template<std::invocable Fn>
    class Defer
    {
    public:
      constexpr Defer(Fn&& f) noexcept : f_(std::move(f)) {}
      constexpr Defer(const Fn& f) : f_(f) {}
      constexpr ~Defer()
      {
        if (!dismissed_)
          f_();
      }

      constexpr void Cancel() noexcept
      {
        dismissed_ = true;
      }

      Defer(const Defer&) = delete;
      Defer(Defer&&) = delete;
      Defer& operator=(const Defer&) = delete;
      Defer& operator=(Defer&&) = delete;

    private:
      bool dismissed_{false};
      Fn f_;
    };

    std::string LoadFile(const std::filesystem::path& path)
    {
      std::ifstream file{path};
      return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
  }

  std::string CompileShaderCppToGlsl(const std::filesystem::path& path)
  {
    std::cout << path << '\n';
    auto compilerConfig = shady::default_compiler_config();
    compilerConfig.specialization.entry_point = "main";

    auto tempPath = std::filesystem::temp_directory_path() / (path.filename().string() + ".ll");

    // Since processes created with popen/system don't inherit our environment (and therefore the working directory), we
    // use an absolute path. This way, both absolute and relative paths should work when calling this function.
    auto absolutePath = std::filesystem::current_path() / path.string();

    auto args = std::stringstream();
    args << "clang++ -std=c++20 -c -emit-llvm -S -g -O0 -ffreestanding -Wno-main-return-type -Xclang "
            "-fpreserve-vec3-type --target=spir64-unknown-unknown ";
    args << "-I \"" FWOG_VCC_INCLUDE_DIR "\" ";
    args << "-D__SHADY__=1 -o " << tempPath.generic_string() << " " << absolutePath;
    
    if (auto err = std::system(args.str().c_str()))
    {
      throw Fwog::ShaderCompilationException("Clang error");
    }

    auto llvm_ir = LoadFile(tempPath.generic_string());

    auto targetConfig = shady::default_target_config();
    auto arenaConfig = shady::default_arena_config(&targetConfig);
    //arenaConfig.address_spaces[shady::AsGlobal].allowed = false;
    auto arena = shady::new_ir_arena(arenaConfig);
    auto d1 = Defer([=] { shady::destroy_ir_arena(arena); });
    auto module = shady::new_module(arena, path.string().c_str());
    if (auto err = shady::driver_load_source_file(&compilerConfig,
                                                  shady::SourceLanguage::SrcLLVM,
                                                  llvm_ir.size(),
                                                  llvm_ir.c_str(),
                                                  nullptr,
                                                  &module))
    {
      throw Fwog::ShaderCompilationException("Shady driver error");
    }

    if (auto err = shady::run_compiler_passes(&compilerConfig, &module))
    {
      throw Fwog::ShaderCompilationException("Shady compiler error");
    }

    const auto emitterConfig = shady::CEmitterConfig{
      .dialect = shady::CDialect_GLSL,
      .explicitly_sized_types = false,
      .allow_compound_literals = false,
      .decay_unsized_arrays = false,
    };

    //shady::set_log_level(0); // Uncomment to dump IR
    auto outputSize = size_t{};
    char* outputBuffer = {};
    shady::emit_c(compilerConfig, emitterConfig, module, &outputSize, &outputBuffer, nullptr);
    auto d2 = Defer([=] { shady::free_output(outputBuffer); });
    
    return {outputBuffer, outputSize};
  }

  std::string CompileShaderCppToGlsl(std::string_view sourceCPP)
  {
    static constexpr auto tempSourceName = "fwog_temp_shader_source.cpp";
    auto tempPath = std::filesystem::temp_directory_path() / tempSourceName;
    {
      auto file = std::ofstream(tempPath);
      file << sourceCPP;
    }
    return CompileShaderCppToGlsl(tempPath);
  }
} // namespace Fwog::detail