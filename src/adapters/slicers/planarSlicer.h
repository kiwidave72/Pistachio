#pragma once
#include "ports/ISlicerPort.h"
#include "core/ConfigStore.h"
#include "ports/IConfigPort.h"

namespace adapters
{


class PlanarSlicer : public ports::ISlicerPort
{
	

public:
	PlanarSlicer() ;

	bool initialize(ports::IConfigPort& config) override;
	
	bool sliceModel(const std::shared_ptr<domain::Model>  model, const std::string& filepath,  ports::SlicerProgressCallback progressCallback = nullptr) override;


private:
	bool m_initialized ;
	ports::IConfigPort* m_config;



};

}