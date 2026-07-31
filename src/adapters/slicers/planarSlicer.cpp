#pragma once
#include "adapters/slicers/PlanarSlicer.h"
 

namespace adapters
{



		PlanarSlicer::PlanarSlicer()
			: m_initialized(false) {
		}

	

		bool PlanarSlicer::initialize(ports::IConfigPort& config) {

			m_config = &config;
			m_initialized = true;
			return true;
		}

		 bool PlanarSlicer::sliceModel(const std::shared_ptr<domain::Model>  model, const std::string& filepath,
			  ports::SlicerProgressCallback  progressCallback) {

			 if (progressCallback)
				 progressCallback("Slicing Model...", 0.0f);


			if (progressCallback)
				progressCallback("Complete!", 100.0f);
			 return true;
		}

}