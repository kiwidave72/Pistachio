#pragma once

#include "Repository.h"



namespace domain::v1 {

    RepositoryFolder::RepositoryFolder(){
		folders = std::vector<RepositoryFolder*>();
		importedAssets = std::vector<RepositoryAsset*>();
	}


	RepositoryFolder::~RepositoryFolder(){
		for (auto f : folders) delete f;
	}

	int RepositoryFolder::totalAssetCount() 
	{
		int n = (int)importedAssets.size();
		for (const auto& f : folders) n += f->totalAssetCount();
		return n;
	}


	Repository:: Repository(){
		folders = std::vector<RepositoryFolder*>();
	}

	Repository::~Repository(){
		for (auto f : folders) delete f;
	}

}