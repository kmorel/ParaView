/*=========================================================================

  Program:   ParaView
  Module:    vtkPVWriter.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2000-2001 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name of Kitware nor the names of any contributors may be used
   to endorse or promote products derived from this software without specific 
   prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "vtkPVWriter.h"

#include "vtkDataSet.h"
#include "vtkObjectFactory.h"
#include "vtkPVApplication.h"
#include "vtkPVProcessModule.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVWriter);
vtkCxxRevisionMacro(vtkPVWriter, "1.6");

//----------------------------------------------------------------------------
vtkPVWriter::vtkPVWriter()
{
  this->InputClassName = 0;
  this->WriterClassName = 0;
  this->Description = 0;
  this->Extension = 0;
  this->Parallel = 0;
  this->DataModeMethod = 0;
}

//----------------------------------------------------------------------------
vtkPVWriter::~vtkPVWriter()
{
  this->SetInputClassName(0);
  this->SetWriterClassName(0);
  this->SetDescription(0);
  this->SetExtension(0);
  this->SetDataModeMethod(0);
}

//----------------------------------------------------------------------------
void vtkPVWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "InputClassName: " 
     << (this->InputClassName?this->InputClassName:"(none)") << endl;
  os << indent << "WriterClassName: " 
     << (this->WriterClassName?this->WriterClassName:"(none)") << endl;
  os << indent << "Description: " 
     << (this->Description?this->Description:"(none)") << endl;
  os << indent << "Extension: " 
     << (this->Extension?this->Extension:"(none)") << endl;
  os << indent << "Parallel: " << this->Parallel << endl;
  os << indent << "DataModeMethod: " 
     << (this->DataModeMethod?this->DataModeMethod:"(none)") << endl;
}

//----------------------------------------------------------------------------
int vtkPVWriter::CanWriteData(vtkDataSet* data, int parallel)
{
  if (data == NULL)
    {
    return 0;
    }
  return (parallel == this->Parallel) && data->IsA(this->InputClassName);
}

//----------------------------------------------------------------------------
vtkPVApplication* vtkPVWriter::GetPVApplication()
{
  return vtkPVApplication::SafeDownCast(this->Application);
}

//----------------------------------------------------------------------------
void vtkPVWriter::Write(const char* fileName, const char* dataTclName,
                        int numProcs, int ghostLevel)
{
  vtkPVApplication* pvApp = this->GetPVApplication();
  
  if(!this->Parallel)
    {
    pvApp->GetProcessModule()->ServerScript("%s writer", this->WriterClassName);
    pvApp->GetProcessModule()->ServerScript("writer SetFileName %s", fileName);
    pvApp->GetProcessModule()->ServerScript("writer SetInput %s", dataTclName);
    if (this->DataModeMethod)
      {
      pvApp->GetProcessModule()->ServerScript("writer %s", this->DataModeMethod);
      }
    pvApp->GetProcessModule()->ServerScript("writer Write");
    pvApp->GetProcessModule()->ServerScript("writer Delete");
    }
  else
    {
    pvApp->GetProcessModule()->ServerScript("%s writer", this->WriterClassName);
    pvApp->GetProcessModule()->ServerScript("writer SetFileName %s", fileName);
    pvApp->GetProcessModule()->ServerScript("writer SetInput %s", dataTclName);
    if(this->DataModeMethod)
      {
      pvApp->GetProcessModule()->ServerScript("writer %s", this->DataModeMethod);
      }
    pvApp->GetProcessModule()->ServerScript("writer SetNumberOfPieces %d", numProcs);
    pvApp->GetProcessModule()->ServerScript("writer SetGhostLevel %d", ghostLevel);
    pvApp->GetProcessModule()->RootScript("writer SetStartPiece 0");
    pvApp->GetProcessModule()->RootScript("writer SetEndPiece 0");
    int i;
    for (i=1; i < numProcs; ++i)
      {
      pvApp->RemoteScript(i, "writer SetStartPiece %d", i);
      pvApp->RemoteScript(i, "writer SetEndPiece %d", i);
      }
    pvApp->GetProcessModule()->ServerScript("writer Write");
    pvApp->GetProcessModule()->ServerScript("writer Delete");
    }
}
