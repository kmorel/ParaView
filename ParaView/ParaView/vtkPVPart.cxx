/*=========================================================================

  Program:   ParaView
  Module:    vtkPVPart.cxx
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
#include "vtkPVPart.h"

#include "vtkDataSetSurfaceFilter.h"
#include "vtkImageData.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkProp3D.h"
#include "vtkPVApplication.h"
#include "vtkPVProcessModule.h"
#include "vtkPVDataInformation.h"
#include "vtkPVConfig.h"
#include "vtkPVRenderView.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRectilinearGrid.h"
#include "vtkStructuredGrid.h"
#include "vtkString.h"
#include "vtkTimerLog.h"
#include "vtkToolkits.h"


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVPart);
vtkCxxRevisionMacro(vtkPVPart, "1.16");

int vtkPVPartCommand(ClientData cd, Tcl_Interp *interp,
                     int argc, char *argv[]);


//----------------------------------------------------------------------------
vtkPVPart::vtkPVPart()
{
  this->CommandFunction = vtkPVPartCommand;

  this->CollectionDecision = 1;
  this->LODCollectionDecision = 1;

  this->Name = NULL;

  this->DataInformation = vtkPVDataInformation::New();
  this->VTKDataTclName = NULL;

  // Used to be in vtkPVActorComposite
  static int instanceCount = 0;

  this->Mapper = NULL;

  this->Property = NULL;
  this->PropertyTclName = NULL;
  this->Prop = NULL;
  this->PropTclName = NULL;
  this->LODDeciTclName = NULL;
  this->MapperTclName = NULL;
  this->LODMapperTclName = NULL;
  this->GeometryTclName = NULL;
  this->CollectTclName = NULL;
  this->LODCollectTclName = NULL;
  this->UpdateSuppressorTclName = NULL;
  this->LODUpdateSuppressorTclName = NULL;

  // Create a unique id for creating tcl names.
  ++instanceCount;
  this->InstanceCount = instanceCount;
}

//----------------------------------------------------------------------------
vtkPVPart::~vtkPVPart()
{  
  // Get rid of the circular reference created by the extent translator.
  // We have a problem with ExtractPolyDataPiece also.
  if (this->VTKDataTclName)
    {
    this->GetPVApplication()->GetProcessModule()->
      ServerScript("%s SetExtentTranslator {}", this->VTKDataTclName);
    }  
    
  this->SetName(NULL);

  this->DataInformation->Delete();
  this->DataInformation = NULL;

  // Used to be in vtkPVActorComposite........
  vtkPVApplication *pvApp = this->GetPVApplication();
    
  if ( pvApp )
    {
    pvApp->BroadcastScript("%s Delete", this->MapperTclName);
    }
  this->SetMapperTclName(NULL);
  this->Mapper = NULL;
  
  if ( pvApp )
    {
    pvApp->BroadcastScript("%s Delete", this->LODMapperTclName);
    }
  this->SetLODMapperTclName(NULL);
  
  if ( pvApp )
    {
    pvApp->BroadcastScript("%s Delete", this->PropTclName);
    }
  this->SetPropTclName(NULL);
  this->Prop = NULL;
  
  if ( pvApp )
    {
    pvApp->BroadcastScript("%s Delete", this->PropertyTclName);
    }
  this->SetPropertyTclName(NULL);
  this->Property = NULL;
  
  if (this->LODDeciTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->LODDeciTclName);
      }
    this->SetLODDeciTclName(NULL);
    }
  
  if (this->GeometryTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->GeometryTclName);
      }
    this->SetGeometryTclName(NULL);
    }

  if (this->UpdateSuppressorTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->UpdateSuppressorTclName);
      }
    this->SetUpdateSuppressorTclName(NULL);
    }

  if (this->LODUpdateSuppressorTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->LODUpdateSuppressorTclName);
      }
    this->SetLODUpdateSuppressorTclName(NULL);
    }

  if (this->CollectTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->CollectTclName);
      }
    this->SetCollectTclName(NULL);
    }
  if (this->LODCollectTclName)
    {
    if ( pvApp )
      {
      pvApp->BroadcastScript("%s Delete", this->LODCollectTclName);
      }
    this->SetLODCollectTclName(NULL);
    }
}


//----------------------------------------------------------------------------
void vtkPVPart::CreateParallelTclObjects(vtkPVApplication *pvApp)
{
  char tclName[100];
  
  this->vtkKWObject::SetApplication(pvApp);
  
  // Create the one geometry filter,
  // which creates the poly data from all data sets.
  sprintf(tclName, "Geometry%d", this->InstanceCount);
  pvApp->BroadcastScript("vtkPVGeometryFilter %s", tclName);
  this->SetGeometryTclName(tclName);
  // Keep track of how long each geometry filter takes to execute.
  pvApp->BroadcastScript("%s AddObserver StartEvent {$Application LogStartEvent "
                         "{Execute Geometry}}", this->GeometryTclName);
  pvApp->BroadcastScript("%s AddObserver EndEvent {$Application LogEndEvent "
                         "{Execute Geometry}}", this->GeometryTclName);


  // Create the decimation filter which branches the LOD pipeline.
  sprintf(tclName, "LODDeci%d", this->InstanceCount);
  pvApp->BroadcastScript("vtkQuadricClustering %s", tclName);
  this->LODDeciTclName = NULL;
  this->SetLODDeciTclName(tclName);
  // Keep track of how long each decimation filter takes to execute.
  pvApp->BroadcastScript("%s AddObserver StartEvent {$Application LogStartEvent {Execute Decimate}}", 
                         this->LODDeciTclName);
  pvApp->BroadcastScript("%s AddObserver EndEvent {$Application LogEndEvent {Execute Decimate}}", 
                         this->LODDeciTclName);
  // The input of course is the geometry filter.
  pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                         this->LODDeciTclName, this->GeometryTclName);
  pvApp->BroadcastScript("%s CopyCellDataOn", this->LODDeciTclName);
  pvApp->BroadcastScript("%s UseInputPointsOn", this->LODDeciTclName);
  pvApp->BroadcastScript("%s UseInternalTrianglesOff", this->LODDeciTclName);
  // These options reduce seams, but makes the decimation too slow.
  //pvApp->BroadcastScript("%s UseFeatureEdgesOn", this->LODDeciTclName);
  //pvApp->BroadcastScript("%s UseFeaturePointsOn", this->LODDeciTclName);
  // This should be changed to origin and spacing determined globally.

  if (1) // We could avoid these filters if one processor and not client server.
    {

    // Create the collection filters which allow small models to render locally.  
    // They also redistributed data for SGI pipes option.
    // ===== Primary branch:
    sprintf(tclName, "Collect%d", this->InstanceCount);

    // Different filter for  pipe redistribution.
    if (pvApp->GetUseRenderingGroup())
      {
      pvApp->BroadcastScript("vtkAllToNRedistributePolyData %s", tclName);
      pvApp->BroadcastScript("%s SetNumberOfProcesses %d", tclName,
                             pvApp->GetNumberOfPipes());
      }
    else if (pvApp->GetUseTiledDisplay())
      {
      int numProcs = pvApp->GetController()->GetNumberOfProcesses();
      int* dims = pvApp->GetTileDimensions();
      pvApp->BroadcastScript("vtkPVDuplicatePolyData %s", tclName);
      pvApp->BroadcastScript("%s InitializeSchedule %d %d", tclName,
                             numProcs, dims[0]*dims[1]);
      // Initialize collection descision here. (When we have rendering module).
      }
    else
      {
      pvApp->BroadcastScript("vtkCollectPolyData %s", tclName);
      }
    this->SetCollectTclName(tclName);
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->CollectTclName, this->GeometryTclName);
    pvApp->BroadcastScript("%s AddObserver StartEvent {$Application LogStartEvent {Execute Collect}}", 
                           this->CollectTclName);
    pvApp->BroadcastScript("%s AddObserver EndEvent {$Application LogEndEvent {Execute Collect}}", 
                           this->CollectTclName);
    //
    // ===== LOD branch:
    sprintf(tclName, "LODCollect%d", this->InstanceCount);

    // Different filter for pipe redistribution.
    if (pvApp->GetUseRenderingGroup())
      {
      pvApp->BroadcastScript("vtkAllToNRedistributePolyData %s", tclName);
      pvApp->BroadcastScript("%s SetNumberOfProcesses %d", tclName,
                             pvApp->GetNumberOfPipes());
      }
    else if (pvApp->GetUseTiledDisplay())
      {
      int numProcs = pvApp->GetController()->GetNumberOfProcesses();
      int* dims = pvApp->GetTileDimensions();
      pvApp->BroadcastScript("vtkPVDuplicatePolyData %s", tclName);
      pvApp->BroadcastScript("%s InitializeSchedule %d %d", tclName,
                             numProcs, dims[0]*dims[1]);
      // Initialize collection descision here. (When we have rendering module).
      }
    else
      {
      pvApp->BroadcastScript("vtkCollectPolyData %s", tclName);
      }
    this->SetLODCollectTclName(tclName);
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->LODCollectTclName, this->LODDeciTclName);
    pvApp->BroadcastScript("%s AddObserver StartEvent {$Application LogStartEvent {Execute LODCollect}}", 
                           this->LODCollectTclName);
    pvApp->BroadcastScript("%s AddObserver EndEvent {$Application LogEndEvent {Execute LODCollect}}", 
                           this->LODCollectTclName);

    // Handle collection setup with client server.
    pvApp->BroadcastScript("%s SetSocketController [ $Application GetSocketController ] ", 
                           this->CollectTclName);
    pvApp->BroadcastScript("%s SetSocketController [ $Application GetSocketController ] ", 
                           this->LODCollectTclName);
    // Special condition to signal the client.
    // Because both processes of the Socket controller think they are 0!!!!
    if (pvApp->GetClientMode())
      {
      this->Script("%s SetController {}", this->CollectTclName);
      this->Script("%s SetController {}", this->LODCollectTclName);
      }
    }



  // Now create the update supressors which keep the renderers/mappers
  // from updating the pipeline.  These are here to ensure that all
  // processes get updated at the same time.
  // ===== Primary branch:
  sprintf(tclName, "UpdateSuppressor%d", this->InstanceCount);
  pvApp->BroadcastScript("vtkPVUpdateSuppressor %s", tclName);
  this->SetUpdateSuppressorTclName(tclName);
  if (this->CollectTclName)
    {
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->UpdateSuppressorTclName, 
                           this->CollectTclName);
    }
  else
    {
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->UpdateSuppressorTclName, 
                           this->GeometryTclName);
    }
  //
  // ===== LOD branch:
  sprintf(tclName, "LODUpdateSuppressor%d", this->InstanceCount);
  pvApp->BroadcastScript("vtkPVUpdateSuppressor %s", tclName);
  this->SetLODUpdateSuppressorTclName(tclName);
  if (this->LODCollectTclName)
    {
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->LODUpdateSuppressorTclName, 
                           this->LODCollectTclName);
    }
  else
    {
    pvApp->BroadcastScript("%s SetInput [%s GetOutput]", 
                           this->LODUpdateSuppressorTclName, 
                           this->LODDeciTclName);
    }


  // Now create the mappers for the two branches.
  // Make a new tcl object.
  // ===== Primary branch:
  sprintf(tclName, "Mapper%d", this->InstanceCount);
  this->Mapper = (vtkPolyDataMapper*)pvApp->MakeTclObject("vtkPolyDataMapper",
                                                          tclName);
  this->MapperTclName = NULL;
  this->SetMapperTclName(tclName);
  pvApp->BroadcastScript("%s UseLookupTableScalarRangeOn", this->MapperTclName);
  pvApp->BroadcastScript("%s SetColorModeToMapScalars", this->MapperTclName);
  pvApp->BroadcastScript("%s SetInput [%s GetOutput]", this->MapperTclName,
                         this->UpdateSuppressorTclName);
  //
  // ===== LOD branch:
  sprintf(tclName, "LODMapper%d", this->InstanceCount);
  pvApp->MakeTclObject("vtkPolyDataMapper", tclName);
  this->LODMapperTclName = NULL;
  this->SetLODMapperTclName(tclName);
  pvApp->BroadcastScript("%s UseLookupTableScalarRangeOn", this->LODMapperTclName);
  pvApp->BroadcastScript("%s SetColorModeToMapScalars", this->LODMapperTclName);
  pvApp->BroadcastScript("%s SetInput [%s GetOutput]", this->LODMapperTclName,
                         this->LODUpdateSuppressorTclName);
  
  
  // Now the two branches merge at the LOD actor.
  sprintf(tclName, "Actor%d", this->InstanceCount);
  this->Prop = (vtkProp*)pvApp->MakeTclObject("vtkPVLODActor", tclName);
  this->SetPropTclName(tclName);

  // Make a new tcl object.
  sprintf(tclName, "Property%d", this->InstanceCount);
  this->Property = (vtkProperty*)pvApp->MakeTclObject("vtkProperty", tclName);
  this->SetPropertyTclName(tclName);
  pvApp->BroadcastScript("%s SetAmbient 0.15", this->PropertyTclName);
  pvApp->BroadcastScript("%s SetDiffuse 0.85", this->PropertyTclName);
  pvApp->BroadcastScript("%s SetProperty %s", this->PropTclName, 
                         this->PropertyTclName);
  pvApp->BroadcastScript("%s SetMapper %s", this->PropTclName, 
                         this->MapperTclName);
  pvApp->BroadcastScript("%s SetLODMapper %s", this->PropTclName,
                         this->LODMapperTclName);
  
  pvApp->GetProcessModule()->InitializePVPartPartition(this);
}


//----------------------------------------------------------------------------
void vtkPVPart::SetCollectionDecision(int v)
{
  vtkPVApplication* pvApp = this->GetPVApplication();

  if (v == this->CollectionDecision)
    {
    return;
    }
  this->CollectionDecision = v;

  if ( this->UpdateSuppressorTclName == NULL )
    {
    vtkErrorMacro("Missing Suppressor.");
    return;
    }

  if (this->CollectTclName)
    {
    if (this->CollectionDecision)
      {
      pvApp->BroadcastScript("%s SetPassThrough 0; %s RemoveAllCaches; %s ForceUpdate", 
                             this->CollectTclName,
                             this->UpdateSuppressorTclName,
                             this->UpdateSuppressorTclName);
      }
    else
      {
      pvApp->BroadcastScript("%s SetPassThrough 1; %s RemoveAllCaches; %s ForceUpdate", 
                             this->CollectTclName,
                             this->UpdateSuppressorTclName,
                             this->UpdateSuppressorTclName);
      }
    }

  this->GetPVApplication()->BroadcastScript(
             "%s RemoveAllCaches", this->UpdateSuppressorTclName);
}

//----------------------------------------------------------------------------
void vtkPVPart::SetLODCollectionDecision(int v)
{
  vtkPVApplication* pvApp = this->GetPVApplication();

  if (v == this->LODCollectionDecision)
    {
    return;
    }
  this->LODCollectionDecision = v;

  if ( this->UpdateSuppressorTclName == NULL )
    {
    vtkErrorMacro("Missing Suppressor.");
    return;
    }

  if (this->LODCollectTclName)
    {
    if (this->LODCollectionDecision)
      {
      pvApp->BroadcastScript("%s SetPassThrough 0; %s RemoveAllCaches; %s ForceUpdate", 
                             this->LODCollectTclName,
                             this->LODUpdateSuppressorTclName,
                             this->LODUpdateSuppressorTclName);
      }
    else
      {
      pvApp->BroadcastScript("%s SetPassThrough 1; %s RemoveAllCaches; %s ForceUpdate", 
                             this->LODCollectTclName,
                             this->LODUpdateSuppressorTclName,
                             this->LODUpdateSuppressorTclName);
      }
    }

}



//----------------------------------------------------------------------------
void vtkPVPart::SetVisibility(int v)
{
  vtkPVApplication* pvApp = this->GetPVApplication();

  if (this->GetPropTclName())
    {
    pvApp->BroadcastScript("%s SetVisibility %d", this->GetPropTclName(), v);
    }

  // Recompute total visibile memory size.
  pvApp->SetTotalVisibleMemorySizeValid(0);
}



//----------------------------------------------------------------------------
void vtkPVPart::GatherDataInformation()
{
  vtkPVApplication *pvApp = this->GetPVApplication();
  vtkPVProcessModule *pm = pvApp->GetProcessModule();

  pm->GatherDataInformation(this->DataInformation, this->LODDeciTclName);

  // Recompute total visibile memory size.
  pvApp->SetTotalVisibleMemorySizeValid(0);

  // Look for a name defined in Field data.
  this->SetName(this->DataInformation->GetName());

  // I would like to have all sources generate names and all filters
  // Pass the field data, but until all is working well, this is a default.
  if (this->Name == NULL)
    {
    char str[100];
    if (this->DataInformation->GetDataSetType() == VTK_POLY_DATA)
      {
      sprintf(str, "Polygonal: %d cells", this->DataInformation->GetNumberOfCells());
      }
    else if (this->DataInformation->GetDataSetType() == VTK_UNSTRUCTURED_GRID)
      {
      sprintf(str, "Unstructured Grid: %d cells", this->DataInformation->GetNumberOfCells());
      }
    else if (this->DataInformation->GetDataSetType() == VTK_IMAGE_DATA)
      {
      int *ext = this->DataInformation->GetExtent();
      sprintf(str, "Uniform Rectilinear: extent (%d, %d) (%d, %d) (%d, %d)", 
              ext[0], ext[1], ext[2], ext[3], ext[4], ext[5]);
      }
    else if (this->DataInformation->GetDataSetType() == VTK_RECTILINEAR_GRID)
      {
      int *ext = this->DataInformation->GetExtent();
      sprintf(str, "Nonuniform Rectilinear: extent (%d, %d) (%d, %d) (%d, %d)", 
              ext[0], ext[1], ext[2], ext[3], ext[4], ext[5]);
      }
    else if (this->DataInformation->GetDataSetType() == VTK_STRUCTURED_GRID)
      {
      int *ext = this->DataInformation->GetExtent();
      sprintf(str, "Curvilinear: extent (%d, %d) (%d, %d) (%d, %d)", 
              ext[0], ext[1], ext[2], ext[3], ext[4], ext[5]);
      }
    else
      {
      sprintf(str, "Part of unknown type");
      }    
    this->SetName(str);
    }
}

//----------------------------------------------------------------------------
void vtkPVPart::ForceUpdate(vtkPVApplication* pvApp)
{
  if ( this->UpdateSuppressorTclName )
    {
    pvApp->BroadcastScript("%s ForceUpdate", this->UpdateSuppressorTclName);
    pvApp->BroadcastScript("%s ForceUpdate", this->LODUpdateSuppressorTclName);
    }
}

//----------------------------------------------------------------------------
void vtkPVPart::SetPVApplication(vtkPVApplication *pvApp)
{
  this->CreateParallelTclObjects(pvApp);
  this->vtkKWObject::SetApplication(pvApp);
}

//----------------------------------------------------------------------------
void vtkPVPart::SetVTKDataTclName(const char* tclName)
{
  vtkPVApplication *pvApp = this->GetPVApplication();
  vtkPVProcessModule* pm = pvApp->GetProcessModule();
  
  if (pvApp == NULL)
    {
    vtkErrorMacro("Set the application before you set the VTKDataTclName.");
    return;
    }
  
  if (this->VTKDataTclName)
    {
    delete [] this->VTKDataTclName;
    this->VTKDataTclName = NULL;
    }
  if (tclName)
    {
    this->VTKDataTclName = new char[strlen(tclName)+1];
    strcpy(this->VTKDataTclName, tclName);
    // Connect the data to the pipeline for displaying the data.
    pm->ServerScript("%s SetInput %s",
                     this->GeometryTclName,
                     this->VTKDataTclName);
    }
}

//----------------------------------------------------------------------------
void vtkPVPart::Update()
{
  vtkPVApplication *pvApp = this->GetPVApplication();
  
  // The mapper has the assignment for this processor.
  pvApp->GetProcessModule()->ServerScript(
      "[%s GetOutput] SetUpdateExtent [%s GetPiece] [%s GetNumberOfPieces]", 
      this->LODDeciTclName, this->MapperTclName, this->MapperTclName);
  pvApp->GetProcessModule()->ServerScript("%s Update", this->LODDeciTclName);
}


//----------------------------------------------------------------------------
void vtkPVPart::InsertExtractPiecesIfNecessary()
{
  vtkPVApplication *pvApp = this->GetPVApplication();
  vtkPVProcessModule* pm = pvApp->GetProcessModule();

  pm->RootScript("%s UpdateInformation", this->VTKDataTclName);
  pm->RootScript("%s GetMaximumNumberOfPieces", this->VTKDataTclName);
  
  if (atoi(pm->GetRootResult()) != 1)
    { // The source can already produce pieces.
    return;
    }
  
  // We are going to create the piece filter with a dummy tcl name,
  // setup the pipeline, and remove tcl's reference to the objects.
  // The vtkData object will be moved to the output of the piece filter.
  if((pm->RootScript("%s IsA vtkPolyData", this->VTKDataTclName),
      atoi(pm->GetRootResult())))
    {
    // Transmit is more efficient, but has the possiblity of hanging.
    // It will hang if all procs do not  call execute.
    if (getenv("PV_LOCK_SAFE") != NULL)
      {
      pm->ServerSimpleScript("vtkExtractPolyDataPiece pvTemp");
      }
    else
      {
      pm->ServerSimpleScript("vtkTransmitPolyDataPiece pvTemp");
      }
    }
  else if((pm->RootScript("%s IsA vtkUnstructuredGrid", this->VTKDataTclName),
           atoi(pm->GetRootResult())))
    {
    // Transmit is more efficient, but has the possiblity of hanging.
    // It will hang if all procs do not  call execute.
    if (getenv("PV_LOCK_SAFE") != NULL)
      {
      pm->ServerSimpleScript("vtkExtractUnstructuredGridPiece pvTemp");
      }
    else
      {
      pm->ServerSimpleScript("vtkTransmitUnstructuredGridPiece pvTemp");
      }
    }
  else
    {
    return;
    }

  // Connect the filter to the pipeline.
  pm->ServerScript("pvTemp SetInput %s", this->VTKDataTclName);
  // Set the output name to point to the new data object.
  // This is a bit of a hack because we have to strip the '$' off of the current name.
  pm->ServerScript("set %s [pvTemp GetOutput]", this->VTKDataTclName+1);
  
  this->Script("%s SetVTKDataTclName {%s}", this->GetTclName(),
               this->VTKDataTclName);
  
  // Now delete Tcl's reference to the piece filter.
  pm->ServerSimpleScript("pvTemp Delete");
}


//----------------------------------------------------------------------------
vtkPVApplication* vtkPVPart::GetPVApplication()
{
  if (this->Application == NULL)
    {
    return NULL;
    }
  
  if (this->Application->IsA("vtkPVApplication"))
    {  
    return (vtkPVApplication*)(this->Application);
    }
  else
    {
    vtkErrorMacro("Bad typecast");
    return NULL;
    } 
}


//----------------------------------------------------------------------------
void vtkPVPart::RemoveAllCaches()
{
  this->GetPVApplication()->BroadcastScript(
             "%s RemoveAllCaches; %s RemoveAllCaches",
             this->UpdateSuppressorTclName, this->LODUpdateSuppressorTclName);
}


//----------------------------------------------------------------------------
// Assume that this method is only called when the part is visible.
// This is like the ForceUpdate method, but uses cached values if possible.
void vtkPVPart::CacheUpdate(int idx, int total)
{
  this->GetPVApplication()->BroadcastScript(
             "%s CacheUpdate %d %d; %s CacheUpdate %d %d",
             this->UpdateSuppressorTclName, idx, total,
             this->LODUpdateSuppressorTclName, idx, total);
}


//----------------------------------------------------------------------------
void vtkPVPart::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Name: " << (this->Name?this->Name:"none") << endl;
  os << indent << "GeometryTclName: " << (this->GeometryTclName?this->GeometryTclName:"none") << endl;
  os << indent << "LODMapperTclName: " << (this->LODMapperTclName?this->LODMapperTclName:"none") << endl;
  os << indent << "Mapper: " << this->GetMapper() << endl;
  os << indent << "MapperTclName: " << (this->MapperTclName?this->MapperTclName:"none") << endl;
  os << indent << "PropTclName: " << (this->PropTclName?this->PropTclName:"none") << endl;
  os << indent << "VTKDataTclName: " << (this->VTKDataTclName?this->VTKDataTclName:"none") << endl;
  os << indent << "PropertyTclName: " << (this->PropertyTclName?this->PropertyTclName:"none") << endl;
  os << indent << "CollectTclName: " << (this->CollectTclName?this->CollectTclName:"none") << endl;
  os << indent << "LODCollectTclName: " << (this->LODCollectTclName?this->LODCollectTclName:"none") << endl;
  os << indent << "LODDeciTclName: " << (this->LODDeciTclName?this->LODDeciTclName:"none") << endl;

  os << indent << "CollectionDecision: " 
     <<  this->CollectionDecision << endl;
  os << indent << "LODCollectionDecision: " 
     <<  this->LODCollectionDecision << endl;


  if (this->UpdateSuppressorTclName)
    {
    os << indent << "UpdateSuppressor: " << this->UpdateSuppressorTclName << endl;
    }
  if (this->LODUpdateSuppressorTclName)
    {
    os << indent << "LODUpdateSuppressor: " << this->LODUpdateSuppressorTclName << endl;
    }
}


  



