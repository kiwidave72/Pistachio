#pragma once
#include "Workspace.h"
 
using namespace domain::v1;

namespace domain
{



	class DataContext {

	public:
		DataContext();
		~DataContext();
		Workspace* m_workspace;

		 
	};

	 
}