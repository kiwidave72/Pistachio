#include "DataContext.h"

using namespace domain::v1;

namespace domain
{
	DataContext::DataContext()
	{
		m_workspace = new Workspace();
	}
	DataContext::~DataContext()
	{
		delete m_workspace;
	}
}