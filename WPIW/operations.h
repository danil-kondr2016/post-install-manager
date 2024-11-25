#pragma once

#include "framework.h"

HRESULT OpRenameFile(char *FilePath, char *NewName);
HRESULT OpMoveFile(char *Source, char *Destination);
HRESULT OpCopySingleFile(char *Source, char *Destination);
HRESULT OpCopyDirectory(char *Source, char *Destination);
HRESULT OpRemoveFile(char *File);
HRESULT OpRemoveDirectory(char *Directory);