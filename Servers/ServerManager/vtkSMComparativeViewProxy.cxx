/*=========================================================================

  Program:   ParaView
  Module:    vtkSMComparativeViewProxy.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMComparativeViewProxy.h"

#include "vtkCollection.h"
#include "vtkMemberFunctionCommand.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkSMCameraLink.h"
#include "vtkSMComparativeAnimationCueProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPropertyIterator.h"
#include "vtkSMProxyLink.h"
#include "vtkSMProxyManager.h"
#include "vtkSMProxyProperty.h"
#include "vtkSMRepresentationProxy.h"

#include <vtkstd/vector>
#include <vtkstd/map>
#include <vtkstd/set>
#include <vtkstd/string>
#include <vtksys/ios/sstream>

#include <assert.h>

//----------------------------------------------------------------------------
static void vtkCopyClone(vtkSMProxy* source, vtkSMProxy* clone,
  vtkstd::set<vtkstd::string> *exceptions=0)
{
  vtkSmartPointer<vtkSMPropertyIterator> iterSource;
  vtkSmartPointer<vtkSMPropertyIterator> iterDest;

  iterSource.TakeReference(source->NewPropertyIterator());
  iterDest.TakeReference(clone->NewPropertyIterator());

  // Since source/clone are exact copies, the following is safe.
  for (; !iterSource->IsAtEnd() && !iterDest->IsAtEnd();
    iterSource->Next(), iterDest->Next())
    {
    // Skip the property if it is in the exceptions list.
    if (exceptions && 
      exceptions->find(iterSource->GetKey()) != exceptions->end())
      {
      continue;
      }
    vtkSMProperty* destProp = iterDest->GetProperty();

    // Skip the property if it is information only or is internal.
    if (destProp->GetInformationOnly())
      {
      continue;
      }

    // Copy the property from source to dest
    destProp->Copy(iterSource->GetProperty());
    }
}

//----------------------------------------------------------------------------
class vtkSMComparativeViewProxy::vtkInternal
{
public:
  struct RepresentationCloneItem
    {
    // The clone representation proxy.
    vtkSmartPointer<vtkSMRepresentationProxy> CloneRepresentation;

    // The sub-view in which this clone exists.
    vtkSmartPointer<vtkSMViewProxy> ViewProxy;

    RepresentationCloneItem() {}
    RepresentationCloneItem(
      vtkSMViewProxy* view, vtkSMRepresentationProxy* repr)
      : CloneRepresentation(repr), ViewProxy(view) {}
    };

  struct RepresentationData
    {
    typedef vtkstd::vector<RepresentationCloneItem> VectorOfClones;
    VectorOfClones Clones;
    vtkSmartPointer<vtkSMProxyLink> Link;

    void MarkRepresentationsModified()
      {
      VectorOfClones::iterator iter;
      for (iter = this->Clones.begin(); iter != this->Clones.end(); ++iter)
        {
        iter->CloneRepresentation->MarkDirty(NULL);
        }
      }
  
    // Returns the representation clone in the given view.
    VectorOfClones::iterator FindRepresentationClone(vtkSMViewProxy* view)
      {
      VectorOfClones::iterator iter;
      for (iter = this->Clones.begin(); iter != this->Clones.end(); ++iter)
        {
        if (iter->ViewProxy == view)
          {
          return iter;
          }
        }
      return this->Clones.end();
      }
    };

  typedef vtkstd::vector<vtkSmartPointer<vtkSMViewProxy> > VectorOfViews;
  VectorOfViews Views;

  typedef vtkstd::map<vtkSMRepresentationProxy*, RepresentationData> MapOfReprClones;
  MapOfReprClones RepresentationClones;

  typedef vtkstd::vector<vtkSmartPointer<vtkSMComparativeAnimationCueProxy> >
    VectorOfCues;
  VectorOfCues Cues;

  vtkSmartPointer<vtkSMProxyLink> ViewLink;
  vtkSmartPointer<vtkSMCameraLink> ViewCameraLink;

  vtkInternal()
    {
    this->ViewLink = vtkSmartPointer<vtkSMProxyLink>::New();
    this->ViewCameraLink = vtkSmartPointer<vtkSMCameraLink>::New();
    this->ViewCameraLink->SynchronizeInteractiveRendersOff();
    }

  // Creates a new representation clone and adds it in the given view.
  // Arguments:
  // @repr -- representation to clone
  // @view -- the view to which the new clone should be added.
  vtkSMRepresentationProxy* AddRepresentationClone(
    vtkSMRepresentationProxy* repr, vtkSMViewProxy* view)
    {
    MapOfReprClones::iterator iter = this->RepresentationClones.find(repr);
    if (iter == this->RepresentationClones.end())
      {
      vtkGenericWarningMacro("This representation cannot be cloned!!!");
      return NULL;
      }

    vtkInternal::RepresentationData& data = iter->second;

    vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();

    // Create a new representation
    vtkSMRepresentationProxy* newRepr = vtkSMRepresentationProxy::SafeDownCast(
      pxm->NewProxy(repr->GetXMLGroup(), repr->GetXMLName()));

    // Made the new representation a clone
    vtkCopyClone(repr, newRepr);
    newRepr->UpdateVTKObjects();
    data.Link->AddLinkedProxy(newRepr, vtkSMLink::OUTPUT);

    // Add the cloned representation to the view
    view->AddRepresentation(newRepr);

    // Add the cloned representation to the RepresentationData struct
    // The clone is added to a map where its view is the key.
    data.Clones.push_back(
      vtkInternal::RepresentationCloneItem(view, newRepr));

    // Clean up this reference
    newRepr->Delete();
    return newRepr;
    }

  unsigned int ActiveIndexX;
  unsigned int ActiveIndexY;
  vtkstd::string SuggestedViewType;
};

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSMComparativeViewProxy);

//----------------------------------------------------------------------------
vtkSMComparativeViewProxy::vtkSMComparativeViewProxy()
{
  this->Internal = new vtkInternal();
  this->Dimensions[0] = 0;
  this->Dimensions[1] = 0;
  this->ViewSize[0] = 400;
  this->ViewSize[1] = 400;
  this->OverlayAllComparisons = false;
  this->Spacing[0] = this->Spacing[1] = 1;

  this->Outdated = true;

  vtkMemberFunctionCommand<vtkSMComparativeViewProxy>* fsO = 
    vtkMemberFunctionCommand<vtkSMComparativeViewProxy>::New();
  fsO->SetCallback(*this, &vtkSMComparativeViewProxy::MarkOutdated);
  this->MarkOutdatedObserver = fsO;
}

//----------------------------------------------------------------------------
vtkSMComparativeViewProxy::~vtkSMComparativeViewProxy()
{
  delete this->Internal;
  this->MarkOutdatedObserver->Delete();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::AddCue(vtkSMComparativeAnimationCueProxy* cue)
{
  this->Internal->Cues.push_back(cue);
  cue->AddObserver(vtkCommand::ModifiedEvent, this->MarkOutdatedObserver);
  this->MarkOutdated();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::RemoveCue(vtkSMComparativeAnimationCueProxy* cue)
{
  vtkInternal::VectorOfCues::iterator iter;
  for (iter = this->Internal->Cues.begin();
    iter != this->Internal->Cues.end(); ++iter)
    {
    if (iter->GetPointer() == cue)
      {
      cue->RemoveObserver(this->MarkOutdatedObserver);
      this->Internal->Cues.erase(iter);
      this->MarkOutdated();
      break;
      }
    }
}

//----------------------------------------------------------------------------
bool vtkSMComparativeViewProxy::BeginCreateVTKObjects()
{
  vtkSMViewProxy* rootView = vtkSMViewProxy::SafeDownCast(
    this->GetSubProxy("RootView"));
  if (!rootView)
    {
    vtkErrorMacro("Subproxy \"Root\" must be defined in the xml configuration.");
    return false;
    }

  this->Dimensions[0] = 1;
  this->Dimensions[1] = 1;

  // Root view is the first view in the views list.
  this->Internal->Views.push_back(rootView);

  this->Internal->ViewCameraLink->AddLinkedProxy(rootView, vtkSMLink::INPUT);
  this->Internal->ViewCameraLink->AddLinkedProxy(rootView, vtkSMLink::OUTPUT);
  this->Internal->ViewLink->AddLinkedProxy(rootView, vtkSMLink::INPUT);

  // Every view keeps their own representations.
  this->Internal->ViewLink->AddException("Representations");

  // This view computes view size/view position for each view based on the
  // layout.
  this->Internal->ViewLink->AddException("ViewSize");
  this->Internal->ViewLink->AddException("ViewTime");
  this->Internal->ViewLink->AddException("ViewPosition");

  this->Internal->ViewLink->AddException("CameraPositionInfo");
  this->Internal->ViewLink->AddException("CameraPosition");
  this->Internal->ViewLink->AddException("CameraFocalPointInfo");
  this->Internal->ViewLink->AddException("CameraFocalPoint");
  this->Internal->ViewLink->AddException("CameraViewUpInfo");
  this->Internal->ViewLink->AddException("CameraViewUp");
  this->Internal->ViewLink->AddException("CameraClippingRangeInfo");
  this->Internal->ViewLink->AddException("CameraClippingRange");
  this->Internal->ViewLink->AddException("CameraViewAngleInfo");
  this->Internal->ViewLink->AddException("CameraViewAngle");

  return this->Superclass::BeginCreateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::SetOverlayAllComparisons(bool overlay)
{
  if (this->OverlayAllComparisons == overlay)
    {
    return;
    }

  this->OverlayAllComparisons = overlay;
  this->Modified();

  this->Build(this->Dimensions[0], this->Dimensions[1]);
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::Build(int dx, int dy)
{
  // Ensure objects are created before building.
  this->CreateVTKObjects();

  if (dx <= 0 || dy <= 0)
    {
    vtkErrorMacro("Dimensions cannot be 0.");
    return;
    }

  size_t numViews = this->OverlayAllComparisons? 1 : dx * dy;
  size_t cc;

  assert(numViews >= 1);

  // Remove extra view modules.
  for (cc=this->Internal->Views.size()-1; cc >= numViews; cc--)
    {
    this->RemoveView(this->Internal->Views[cc]);
    this->Outdated = true;
    }

  // Add view modules, if not enough.
  for (cc=this->Internal->Views.size(); cc < numViews; cc++)
    {
    this->AddNewView();
    this->Outdated = true;
    }

  this->Dimensions[0] = dx;
  this->Dimensions[1] = dy;

  if (this->OverlayAllComparisons)
    {
    // ensure that there are enough representation clones in the root view to
    // match the dimensions.

    vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
    vtkSMViewProxy* root_view = this->GetRootView();
    size_t numReprs = dx * dy;
    vtkInternal::MapOfReprClones::iterator reprIter;
    for (reprIter = this->Internal->RepresentationClones.begin();
      reprIter != this->Internal->RepresentationClones.end(); ++reprIter)
      {
      vtkSMRepresentationProxy* repr = reprIter->first;
      vtkInternal::RepresentationData& data = reprIter->second;

      // remove old root-clones if extra.
      if (data.Clones.size() > numReprs)
        {
        for (cc = data.Clones.size()-1; cc >= numReprs; cc--)
          {
          vtkSMRepresentationProxy* root_clone =
            data.Clones[cc].CloneRepresentation;
          root_view->RemoveRepresentation(root_clone);
          data.Link->RemoveLinkedProxy(root_clone);
          }
        data.Clones.resize(numReprs);
        }
      else
        {
        // add new root-clones if needed.
        for (cc = data.Clones.size(); cc < numReprs-1; cc++)
          {
          vtkSMRepresentationProxy* newRepr = vtkSMRepresentationProxy::SafeDownCast(
            pxm->NewProxy(repr->GetXMLGroup(), repr->GetXMLName()));
          vtkCopyClone(repr, newRepr); // create a clone
          newRepr->UpdateVTKObjects(); // create objects
          data.Link->AddLinkedProxy(newRepr, vtkSMLink::OUTPUT); // link properties
          root_view->AddRepresentation(newRepr);  // add representation to view

          // Now update data structure to include this view/repr clone.
          data.Clones.push_back(
            vtkInternal::RepresentationCloneItem(root_view, newRepr));
          newRepr->Delete();
          }
        }
      }
    }

  this->UpdateViewLayout();

  // Whenever the layout changes we'll fire the ConfigureEvent.
  this->InvokeEvent(vtkCommand::ConfigureEvent);
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::UpdateViewLayout()
{
  int dx = this->OverlayAllComparisons? 1 : this->Dimensions[0];
  int dy = this->OverlayAllComparisons? 1 : this->Dimensions[1];
  int width = 
    (this->ViewSize[0] - (dx-1)*this->Spacing[0])/dx;
  int height = 
    (this->ViewSize[1] - (dy-1)*this->Spacing[1])/dy;

  size_t view_index = 0;
  for (int y=0; y < dy; ++y)
    {
    for (int x=0; x < dx; ++x, ++view_index)
      {
      vtkSMViewProxy* view = this->Internal->Views[view_index];
      int view_pos[2];
      view_pos[0] = this->ViewPosition[0] + width * x;
      view_pos[1] = this->ViewPosition[1] + height * y;
      vtkSMPropertyHelper(view, "ViewPosition").Set(view_pos, 2);

      // Not all view classes have a ViewSize property
      vtkSMPropertyHelper(view, "ViewSize", true).Set(0, width);
      vtkSMPropertyHelper(view, "ViewSize", true).Set(1, height);

      vtkSMPropertyHelper(view, "GUISize").Set(this->GUISize, 2);

      view->UpdateVTKObjects();
      }
    }
}

//----------------------------------------------------------------------------
vtkSMViewProxy* vtkSMComparativeViewProxy::GetRootView()
{
  if (this->Internal->Views.size())
    {
    return this->Internal->Views[0];
    }
  else
    {
    return 0;
    }
}
//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::AddNewView()
{

  vtkSMViewProxy* rootView = this->GetRootView();

  vtkSMProxyManager* pxm = vtkSMProxyManager::GetProxyManager();
  vtkSMViewProxy* newView = vtkSMViewProxy::SafeDownCast(
    pxm->NewProxy(rootView->GetXMLGroup(), rootView->GetXMLName()));
  if (!newView)
    {
    vtkErrorMacro("Failed to create internal view proxy. Comparative visualization "
      "view cannot work.");
    return;
    }

  newView->SetConnectionID(this->ConnectionID);
  newView->UpdateVTKObjects();

  // Copy current view properties over to this newly created view.
  vtkstd::set<vtkstd::string> exceptions;
  exceptions.insert("Representations");
  exceptions.insert("ViewSize");
  exceptions.insert("ViewPosition");
  vtkCopyClone(rootView, newView, &exceptions);

  this->Internal->Views.push_back(newView);
  this->Internal->ViewCameraLink->AddLinkedProxy(newView, vtkSMLink::INPUT);
  this->Internal->ViewCameraLink->AddLinkedProxy(newView, vtkSMLink::OUTPUT);
  this->Internal->ViewLink->AddLinkedProxy(newView, vtkSMLink::OUTPUT);
  newView->Delete();

  // Create clones for all currently added representation for the new view.
  vtkInternal::MapOfReprClones::iterator reprIter;
  for (reprIter = this->Internal->RepresentationClones.begin();
    reprIter != this->Internal->RepresentationClones.end(); ++reprIter)
    {
    vtkSMRepresentationProxy* repr = reprIter->first;

    vtkSMRepresentationProxy* clone =
      this->Internal->AddRepresentationClone(repr, newView);
    assert(clone != NULL);
    }
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::RemoveView(vtkSMViewProxy* view)
{
  if (view == this->GetRootView())
    {
    vtkErrorMacro("Root view cannot be removed.");
    return;
    }

  // Remove all representations in this view.
  vtkInternal::MapOfReprClones::iterator reprIter;
  for (reprIter = this->Internal->RepresentationClones.begin();
    reprIter != this->Internal->RepresentationClones.end(); ++reprIter)
    {
    vtkInternal::RepresentationData& data = reprIter->second;
    vtkInternal::RepresentationData::VectorOfClones::iterator cloneIter =
      data.FindRepresentationClone(view);
    if (cloneIter != data.Clones.end())
      {
      vtkSMRepresentationProxy* clone = cloneIter->CloneRepresentation;
      view->RemoveRepresentation(clone);
      data.Link->RemoveLinkedProxy(clone);
      data.Clones.erase(cloneIter);
      }
    }

  this->Internal->ViewLink->RemoveLinkedProxy(view);
  this->Internal->ViewCameraLink->RemoveLinkedProxy(view);
  this->Internal->ViewCameraLink->RemoveLinkedProxy(view);

  vtkInternal::VectorOfViews::iterator iter;
  for (iter = this->Internal->Views.begin(); 
       iter != this->Internal->Views.end(); ++iter)
    {
    if (iter->GetPointer() == view)
      {
      this->Internal->Views.erase(iter);
      break;
      }
    }
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::AddRepresentation(vtkSMRepresentationProxy* repr)
{
  if (!repr)
    {
    return;
    }

  this->MarkOutdated();

  // Add representation to the root view
  vtkSMViewProxy* rootView = this->GetRootView();
  rootView->AddRepresentation(repr);

  // Create clones of representation and add them to all other views.
  // We will save information about the clones we create
  // so that we can clean them up later.
  vtkInternal::RepresentationData data;

  // We'll link the clones' properties to the original
  // representation using a proxy link.  The "UpdateTime"
  // property will not be linked however.
  vtkSMProxyLink* reprLink = vtkSMProxyLink::New();
  data.Link.TakeReference(reprLink);
  reprLink->AddLinkedProxy(repr, vtkSMLink::INPUT);
  reprLink->AddException("UpdateTime");

  // Add the RepresentationData struct to a map
  // with the original representation as the key
  this->Internal->RepresentationClones[repr] = data;


  // Now, for all existing sub-views, create representation clones.
  vtkInternal::VectorOfViews::iterator iter = this->Internal->Views.begin();
  iter++; // skip root view.
  for (; iter != this->Internal->Views.end(); ++iter)
    {
    // Create a new representation
    vtkSMRepresentationProxy* newRepr = 
      this->Internal->AddRepresentationClone(
        repr, iter->GetPointer());
    (void)newRepr;
    assert(newRepr != NULL);
    }

  if (this->OverlayAllComparisons)
    {
    size_t numReprs = this->Dimensions[0] * this->Dimensions[1];
    for (size_t cc=1; cc < numReprs; cc++)
      {
      // Create a new representation
      vtkSMRepresentationProxy* newRepr =
        this->Internal->AddRepresentationClone(repr, rootView);
      (void)newRepr;
      assert(newRepr);
      }
    }

  // Signal that representations have changed
  this->InvokeEvent(vtkCommand::UserEvent);

  // Override superclass' AddRepresentation since  repr has already been added
  // to the rootView.
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::RemoveRepresentation(vtkSMRepresentationProxy* repr)
{
  vtkInternal::MapOfReprClones::iterator reprDataIter
    = this->Internal->RepresentationClones.find(repr);
  if (!repr || reprDataIter == this->Internal->RepresentationClones.end())
    {
    // Nothing to do.
    return;
    }

  this->MarkOutdated();

  vtkInternal::RepresentationData& data = reprDataIter->second;

  // Remove all clones of this representation.
  vtkInternal::RepresentationData::VectorOfClones::iterator cloneIter;
  for (cloneIter = data.Clones.begin(); cloneIter != data.Clones.end(); ++cloneIter)
    {
    vtkSMViewProxy* view = cloneIter->ViewProxy;
    vtkSMRepresentationProxy* clone = cloneIter->CloneRepresentation;
    if (view && clone)
      {
      view->RemoveRepresentation(clone);
      // No need to clean the clone from the proxy link since the link object
      // will be destroyed anyways.
      }
    }

  // This will destroy the repr proxy link as well.
  this->Internal->RepresentationClones.erase(reprDataIter);

  // Remove repr from RootView.
  vtkSMViewProxy* rootView = this->GetRootView();
  rootView->RemoveRepresentation(repr);

  // Signal that representations have changed
  this->InvokeEvent(vtkCommand::UserEvent);

  // Override superclass' RemoveRepresentation since  repr was not added to this
  // view at all, we added it to (and removed from) the root view.
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::RemoveAllRepresentations()
{
  vtkInternal::MapOfReprClones::iterator iter = 
    this->Internal->RepresentationClones.begin();
  while (iter != this->Internal->RepresentationClones.end())
    {
    vtkSMRepresentationProxy* repr = iter->first;
    this->RemoveRepresentation(repr);
    iter = this->Internal->RepresentationClones.begin();
    }

  this->MarkOutdated();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::MarkDirty(vtkSMProxy* modifiedProxy) 
{
  // The representation that gets added to this view is a consumer of it's
  // input. While this view is a consumer of the representation. So, when the
  // input source is modified, that call eventually leads to
  // vtkSMComparativeViewProxy::MarkDirty(). When that happens, we need to
  // ensure that we regenerate the comparison, so we call this->MarkOutdated().

  // TODO: We can be even smarter. We may want to try to consider only those
  // representations that are actually involved in the parameter comparison to
  // mark this view outdated. This will save on the regeneration of cache when
  // not needed.

  // TODO: Another optimization: we can enable caching only for those
  // representations that are invovled in parameter comparison, others we don't
  // even need to cache.

  // TODO: Need to update data ranges by collecting ranges from all views.
  this->Superclass::MarkDirty(modifiedProxy);
  this->MarkOutdated();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::EndStillRender()
{
  // The StillRender will propagate through the ViewCameraLink to all the other
  // views.
  this->GetRootView()->StillRender();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::EndInteractiveRender()
{
  // The InteractiveRender will propagate through the ViewCameraLink to all the 
  // other views.
  this->GetRootView()->InteractiveRender();
}

//----------------------------------------------------------------------------
vtkSMRepresentationProxy* vtkSMComparativeViewProxy::CreateDefaultRepresentation(
  vtkSMProxy* src, int outputport)
{
  return this->GetRootView()->CreateDefaultRepresentation(src, outputport);
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::UpdateAllRepresentations()
{
  if (!this->Outdated)
    {
    // cout << "Not Outdated" << endl;
    return;
    }

  // ensure that all representation caches are cleared.
  this->ClearDataCaches();
  // cout << "-------------" << endl;

  vtkSMComparativeAnimationCueProxy* timeCue = NULL;
  // locate time cue.
  for (vtkInternal::VectorOfCues::iterator iter = this->Internal->Cues.begin();
    iter != this->Internal->Cues.end(); ++iter)
    {
    // for now, we are saying that the first cue that has no animatable  proxy
    // is for animating time.
    if (iter->GetPointer()->GetAnimatedProxy() == NULL)
      {
      timeCue = iter->GetPointer();
      break;
      }
    }

  int index = 0;
  for (int y=0; y < this->Dimensions[1]; y++)
    {
    for (int x=0; x < this->Dimensions[0]; x++)
      {
      int view_index = this->OverlayAllComparisons? 0 : index;
      vtkSMViewProxy* view = this->Internal->Views[view_index];

      if (timeCue)
        {
        double value = timeCue->GetValue(
          x, y, this->Dimensions[0], this->Dimensions[1]);
        view->SetViewUpdateTime(value);
        }
      else
        {
        view->SetViewUpdateTime(this->GetViewUpdateTime());
        }
      view->SetCacheTime(0);

      for (vtkInternal::VectorOfCues::iterator iter =
        this->Internal->Cues.begin();
        iter != this->Internal->Cues.end(); ++iter)
        {
        if (iter->GetPointer() == timeCue)
          {
          continue;
          }
        iter->GetPointer()->UpdateAnimatedValue(
          x, y, this->Dimensions[0], this->Dimensions[1]);
        }

      // Make the view cache the current setup. 
      this->UpdateAllRepresentations(x, y);

      index++;
      }
    }

  this->Outdated = false;
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::UpdateAllRepresentations(int x, int y)
{
  vtkSMViewProxy* view = this->OverlayAllComparisons?
    this->Internal->Views[0] :
    this->Internal->Views[y*this->Dimensions[0] + x];

  if (!this->OverlayAllComparisons)
    {
    view->UpdateAllRepresentations();
    return;
    }

  vtkCollection* collection = vtkCollection::New();
  this->GetRepresentations(x, y, collection);
  collection->InitTraversal();
  while (vtkSMRepresentationProxy* repr = 
    vtkSMRepresentationProxy::SafeDownCast(
      collection->GetNextItemAsObject()))
    {
    if (repr->GetVisibility())
      {
      repr->Update(view);
      }
    }
  collection->Delete();
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::ClearDataCaches()
{
  vtkInternal::VectorOfViews::iterator viter;
  for (viter = this->Internal->Views.begin(); 
    viter != this->Internal->Views.end(); ++viter)
    {
    viter->GetPointer()->SetUseCache(false);
    }

  // Mark all representations modified. This clears their caches. It's essential
  // that SetUseCache(false) is called before we do this, otherwise the caches
  // are not cleared.

  vtkInternal::MapOfReprClones::iterator repcloneiter;
  for (repcloneiter = this->Internal->RepresentationClones.begin();
    repcloneiter != this->Internal->RepresentationClones.end();
    ++repcloneiter)
    {
    repcloneiter->first->MarkDirty(NULL);
    repcloneiter->second.MarkRepresentationsModified();
    }

  for (viter = this->Internal->Views.begin(); 
    viter != this->Internal->Views.end(); ++viter)
    {
    viter->GetPointer()->SetUseCache(true);
    }
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::GetViews(vtkCollection* collection)
{
  if (!collection)
    {
    return;
    }

  vtkInternal::VectorOfViews::iterator iter;
  for (iter = this->Internal->Views.begin(); 
       iter != this->Internal->Views.end(); ++iter)
    {
    collection->AddItem(iter->GetPointer());
    }
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::GetRepresentationsForView(vtkSMViewProxy* view, 
      vtkCollection *collection)
{
  if (!collection)
    {
    return;
    }

  // Find all representations in this view.

  // For each representation
  vtkInternal::MapOfReprClones::iterator reprIter;
  for (reprIter = this->Internal->RepresentationClones.begin();
    reprIter != this->Internal->RepresentationClones.end(); ++reprIter)
    {

    if (view == this->GetRootView())
      {
      // If the view is the root view, then its representations
      // are the keys of map we are iterating over
      collection->AddItem(reprIter->first);
      continue;
      }

    // The view is not the root view, so it could be one of the cloned views.
    // Search the RepresentationData struct for a representation
    // belonging to the cloned view.
    vtkInternal::RepresentationData& data = reprIter->second;
    vtkInternal::RepresentationData::VectorOfClones::iterator cloneIter
      = data.FindRepresentationClone(view);
    if (cloneIter != data.Clones.end())
      {
      // A representation was found, so add it to the collection.
      vtkSMRepresentationProxy* repr = cloneIter->CloneRepresentation;
      collection->AddItem(repr);
      }
    }

}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::GetRepresentations(int x, int y,
  vtkCollection* collection)
{
  if (!collection)
    {
    return;
    }

  vtkSMViewProxy* view = this->OverlayAllComparisons?
    this->Internal->Views[0] :
    this->Internal->Views[y*this->Dimensions[0] + x];

  int index = y * this->Dimensions[0] + x;

  if (!this->OverlayAllComparisons)
    {
    this->GetRepresentationsForView(view, collection);
    return;
    }

  vtkInternal::MapOfReprClones::iterator reprIter;
  for (reprIter = this->Internal->RepresentationClones.begin();
    reprIter != this->Internal->RepresentationClones.end(); ++reprIter)
    {
    vtkInternal::RepresentationData& data = reprIter->second;
    if (index == 0)
      {
      collection->AddItem(reprIter->first);  
      }
    else
      {
      collection->AddItem(data.Clones[index-1].CloneRepresentation);
      }
    }
}

//----------------------------------------------------------------------------
const char* vtkSMComparativeViewProxy::GetSuggestedViewType(vtkIdType connectionID)
{
  vtkSMViewProxy* rootView = vtkSMViewProxy::SafeDownCast(this->GetSubProxy("RootView"));
  if (rootView)
    {
    vtksys_ios::ostringstream stream;
    stream << "Comparative" << rootView->GetSuggestedViewType(connectionID);
    this->Internal->SuggestedViewType = stream.str();
    return this->Internal->SuggestedViewType.c_str();
    }

  return this->Superclass::GetSuggestedViewType(connectionID);
}

//----------------------------------------------------------------------------
void vtkSMComparativeViewProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Dimensions: " << this->Dimensions[0] 
    << ", " << this->Dimensions[1] << endl;
  os << indent << "Spacing: " << this->Spacing[0] << ", "
    << this->Spacing[1] << endl;
}

