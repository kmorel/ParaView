/*=========================================================================

  Program:   ParaView
  Module:    vtkTimeToTextConvertor.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkTimeToTextConvertor
// .SECTION Description
//

#ifndef __vtkTimeToTextConvertor_h
#define __vtkTimeToTextConvertor_h

#include "vtkTableAlgorithm.h"

class VTK_EXPORT vtkTimeToTextConvertor : public vtkTableAlgorithm
{
public:
  static vtkTimeToTextConvertor* New();
  vtkTypeRevisionMacro(vtkTimeToTextConvertor, vtkTableAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Get/Set the format in which the to display the
  // input update time. Use printf formatting.
  // Default is "Time: %f".
  vtkSetStringMacro(Format);
  vtkGetStringMacro(Format);

// BTX
protected:
  vtkTimeToTextConvertor();
  ~vtkTimeToTextConvertor();

  virtual int FillInputPortInformation(int port, vtkInformation* info);
  virtual int RequestData(vtkInformation* request,
                          vtkInformationVector** inputVector,
                          vtkInformationVector* outputVector);

  char* Format;
private:
  vtkTimeToTextConvertor(const vtkTimeToTextConvertor&); // Not implemented
  void operator=(const vtkTimeToTextConvertor&); // Not implemented
//ETX
};

#endif

