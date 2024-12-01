#pragma once

#include "stdafx.h"

HRESULT OpRenameFile(char *FilePath, char *NewName);
HRESULT OpMoveFile(char *Source, char *Destination);
HRESULT OpCopySingleFile(char *Source, char *Destination);
HRESULT OpCopyDirectory(char *Source, char *Destination);
HRESULT OpRemoveFile(char *File);
HRESULT OpRemoveRecurse(char *Directory);
HRESULT OpMakeDirectory(char *Directory);
HRESULT OpRemoveDirectory(char *Directory);

HRESULT OpRegImport(char *RegistryFile);