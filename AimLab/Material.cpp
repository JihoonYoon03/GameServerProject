#include "pch.h"
#include "Material.h"

void Material::Apply() const
{
	shader->Use();
	shader->Set("materialColor", color);
	shader->Set("shininess", shininess);
	shader->Set("unlit", static_cast<int>(unlit));
	shader->Set("ambientOnly", static_cast<int>(ambientOnly));
	shader->Set("useTexture", static_cast<int>(useTexture && texture != nullptr));
	shader->Set("colorTexture", 0);

	if (texture)
	{
		texture->Bind();
	}
	else
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	if (doubleSided)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
	}
}
