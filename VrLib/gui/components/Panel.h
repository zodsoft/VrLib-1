#pragma once

#include "ContainerComponent.h"

namespace vrlib
{
	namespace gui
	{
		namespace components
		{
			class Panel : public ContainerComponent
			{
			public:
				Panel();

				virtual void draw(const glm::mat4 &parentMatrix) override;


			};
		}
	}
}