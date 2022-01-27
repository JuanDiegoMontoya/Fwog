//#include <gsdf/Common.h>
//#include <gsdf/Shader.h>
//#include <glm/gtc/type_ptr.hpp>
//
//namespace GFX
//{
//  void Shader::Bind() const
//  {
//    glUseProgram(id_);
//  }
//
//  void Shader::SetBool(hashed_string uniform, bool value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1i(id_, uniformIDs_[uniform], static_cast<int>(value));
//  }
//  void Shader::SetInt(hashed_string uniform, int value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1i(id_, uniformIDs_[uniform], value);
//  }
//  void Shader::SetUInt(hashed_string uniform, unsigned int value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1ui(id_, uniformIDs_[uniform], value);
//  }
//  void Shader::SetFloat(hashed_string uniform, float value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1f(id_, uniformIDs_[uniform], value);
//  }
//  void Shader::Set1FloatArray(hashed_string uniform, std::span<const float> value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1fv(id_, uniformIDs_[uniform], static_cast<GLsizei>(value.size()), value.data());
//  }
//  void Shader::Set2FloatArray(hashed_string uniform, std::span<const glm::vec2> value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform2fv(id_, uniformIDs_[uniform], static_cast<GLsizei>(value.size()), glm::value_ptr(value.front()));
//  }
//  void Shader::Set3FloatArray(hashed_string uniform, std::span<const glm::vec3> value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform3fv(id_, uniformIDs_[uniform], static_cast<GLsizei>(value.size()), glm::value_ptr(value.front()));
//  }
//  void Shader::Set4FloatArray(hashed_string uniform, std::span<const glm::vec4> value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform4fv(id_, uniformIDs_[uniform], static_cast<GLsizei>(value.size()), glm::value_ptr(value.front()));
//  }
//  void Shader::SetIntArray(hashed_string uniform, std::span<const int> value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform1iv(id_, uniformIDs_[uniform], static_cast<GLsizei>(value.size()), value.data());
//  }
//  void Shader::SetVec2(hashed_string uniform, const glm::vec2& value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform2fv(id_, uniformIDs_[uniform], 1, glm::value_ptr(value));
//  }
//  void Shader::SetIVec2(hashed_string uniform, const glm::ivec2& value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform2iv(id_, uniformIDs_[uniform], 1, glm::value_ptr(value));
//  }
//  void Shader::SetIVec3(hashed_string uniform, const glm::ivec3& value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform3iv(id_, uniformIDs_[uniform], 1, glm::value_ptr(value));
//  }
//  void Shader::SetVec3(hashed_string uniform, const glm::vec3& value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform3fv(id_, uniformIDs_[uniform], 1, glm::value_ptr(value));
//  }
//  void Shader::SetVec4(hashed_string uniform, const glm::vec4& value)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniform4fv(id_, uniformIDs_[uniform], 1, glm::value_ptr(value));
//  }
//  void Shader::SetMat3(hashed_string uniform, const glm::mat3& mat)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniformMatrix3fv(id_, uniformIDs_[uniform], 1, GL_FALSE, glm::value_ptr(mat));
//  }
//  void Shader::SetMat4(hashed_string uniform, const glm::mat4& mat)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniformMatrix4fv(id_, uniformIDs_[uniform], 1, GL_FALSE, glm::value_ptr(mat));
//  }
//  void Shader::SetMat4Array(hashed_string uniform, std::span<const glm::mat4> mats)
//  {
//    ASSERT(uniformIDs_.contains(uniform));
//    glProgramUniformMatrix4fv(id_, uniformIDs_[uniform], mats.size(), GL_FALSE, glm::value_ptr(mats[0]));
//  }
//
//  //void SetHandle(hashed_string uniform, const uint64_t handle)
//  //{
//  //  assert(Uniforms.find(uniform) != Uniforms.end());
//  //  glProgramUniformHandleui64ARB(programID, Uniforms[uniform], handle);
//  //}
//  //void SetHandleArray(hashed_string uniform, std::span<const uint64_t> handles)
//  //{
//  //  assert(Uniforms.find(uniform) != Uniforms.end());
//  //  glProgramUniformHandleui64vARB(programID, Uniforms[uniform], static_cast<GLsizei>(handles.size()), handles.data());
//  //}
//}