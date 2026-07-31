#include "buildplate.h"

namespace domain::v1
{
	BuildPlate::BuildPlate()
	{
		buildPlateModel = std::make_shared<domain::v1::Model>();

		
		modelInstances = std::vector<std::unique_ptr<ModelInstance>>();



	}
	BuildPlate::~BuildPlate()
	{
	 
 	}

}