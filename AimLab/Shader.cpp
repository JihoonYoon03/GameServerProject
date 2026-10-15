#include "pch.h"
#include "Shader.h"

namespace
{
	GLuint Compile(GLenum type, const std::string& filename)
	{
		std::ifstream file(filename);

		if (!file)
		{
			throw std::runtime_error("Cannot open shader: " + filename);
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		const std::string text = buffer.str();
		const char* source = text.c_str();
		const GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);
		GLint success = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			char log[4096]{};
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			glDeleteShader(shader);
			throw std::runtime_error(filename + ": " + log);
		}

		return shader;
	}
} // namespace

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
	const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexPath);
	GLuint fragment = 0;

	try
	{
		fragment = Compile(GL_FRAGMENT_SHADER, fragmentPath);
	}
	catch (...)
	{
		glDeleteShader(vertex);
		throw;
	}

	program_ = glCreateProgram();
	glAttachShader(program_, vertex);
	glAttachShader(program_, fragment);
	glLinkProgram(program_);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint success = GL_FALSE;
	glGetProgramiv(program_, GL_LINK_STATUS, &success);

	if (!success)
	{
		char log[4096]{};
		glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
		glDeleteProgram(program_);
		program_ = 0;
		throw std::runtime_error(std::string("Shader link failed: ") + log);
	}
}

Shader::~Shader()
{
	if (program_)
	{
		glDeleteProgram(program_);
	}
}

void Shader::Use() const
{
	glUseProgram(program_);
}

GLint Shader::Location(const std::string& name) const
{
	const auto found = locations_.find(name);

	if (found != locations_.end()) return found->second;

	const GLint location = glGetUniformLocation(program_, name.c_str());
	locations_[name] = location;

	return location;
}

void Shader::Set(const std::string& name, int value) const
{
	glUniform1i(Location(name), value);
}

void Shader::Set(const std::string& name, float value) const
{
	glUniform1f(Location(name), value);
}

void Shader::Set(const std::string& name, const glm::vec3& value) const
{
	glUniform3fv(Location(name), 1, glm::value_ptr(value));
}

void Shader::Set(const std::string& name, const glm::mat4& value) const
{
	glUniformMatrix4fv(Location(name), 1, GL_FALSE, glm::value_ptr(value));
}
