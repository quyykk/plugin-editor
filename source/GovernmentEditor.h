// SPDX-License-Identifier: GPL-3.0

#ifndef GOVERNMENT_EDITOR_H_
#define GOVERNMENT_EDITOR_H_

#include "Government.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the government editor window.
class GovernmentEditor : public TemplateEditor<Government> {
public:
	GovernmentEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Government *government) const;

private:
	void RenderGovernment();
};



#endif
