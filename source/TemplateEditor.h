// SPDX-License-Identifier: GPL-3.0

#ifndef TEMPLATE_EDITOR_H_
#define TEMPLATE_EDITOR_H_

#include "Body.h"
#include "DataWriter.h"
#include "Effect.h"
#include "Fleet.h"
#include "GameData.h"
#include "Minable.h"
#include "RandomEvent.h"
#include "Sound.h"
#include "Sprite.h"
#include "System.h"
#include "WeightedList.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_stdlib.h"

#include <functional>
#include <iomanip>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class Editor;



template <typename T> constexpr const char *defaultFileFor() = delete;
template <> constexpr const char *defaultFileFor<Effect>() { return "effects.txt"; }
template <> constexpr const char *defaultFileFor<Fleet>() { return "fleets.txt"; }
template <> constexpr const char *defaultFileFor<Galaxy>() { return "map.txt"; }
template <> constexpr const char *defaultFileFor<Hazard>() { return "hazards.txt"; }
template <> constexpr const char *defaultFileFor<Government>() { return "governments.txt"; }
template <> constexpr const char *defaultFileFor<Outfit>() { return "outfits.txt"; }
template <> constexpr const char *defaultFileFor<Sale<Outfit>>() { return "outfitters.txt"; }
template <> constexpr const char *defaultFileFor<Planet>() { return "map.txt"; }
template <> constexpr const char *defaultFileFor<Ship>() { return "ships.txt"; }
template <> constexpr const char *defaultFileFor<Sale<Ship>>() { return "shipyards.txt"; }
template <> constexpr const char *defaultFileFor<System>() { return "map.txt"; }

namespace impl {
template <typename T>
std::string GetName(const T &obj, ...) { return obj.Name(); }
template <typename T>
std::string GetName(const T &obj, decltype(std::declval<T>().TrueName(), int()) *) { return obj.TrueName(); }
}
template <typename T>
std::string GetName(const T &obj) { return impl::GetName<T>(obj, 0); }

// Base class common for any editor window.
template <typename T>
class TemplateEditor {
public:
	TemplateEditor(Editor &editor, bool &show) noexcept
		: editor(editor), show(show) {}
	TemplateEditor(const TemplateEditor &) = delete;
	TemplateEditor& operator=(const TemplateEditor &) = delete;

	void Clear();


protected:
	// Marks the current object as dirty.
	void SetDirty();
	void SetDirty(const T *obj);

	void RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map);
	bool RenderElement(Body *sprite, const std::string &name, const std::function<bool(const std::string &)> &spriteFilter = {});
	void RenderSound(const std::string &name, std::map<const Sound *, int> &map);
	void RenderEffect(const std::string &name, std::map<const Effect *, int> &map);


protected:
	Editor &editor;
	bool &show;

	std::string searchBox;
	T *object = nullptr;
};


extern template class TemplateEditor<Effect>;
extern template class TemplateEditor<Fleet>;
extern template class TemplateEditor<Galaxy>;
extern template class TemplateEditor<Government>;
extern template class TemplateEditor<Hazard>;
extern template class TemplateEditor<Outfit>;
extern template class TemplateEditor<Sale<Outfit>>;
extern template class TemplateEditor<Planet>;
extern template class TemplateEditor<Ship>;
extern template class TemplateEditor<Sale<Ship>>;
extern template class TemplateEditor<System>;


inline const std::string &NameFor(const std::string &obj) { return obj; }
inline const std::string &NameFor(const std::string *obj) { return *obj; }
template <typename T>
const std::string &NameFor(const T &obj) { return obj.Name(); }
template <typename T>
const std::string &NameFor(const T *obj) { return obj->Name(); }
template <typename T, typename U>
std::string NameFor(const std::pair<T, U> &obj) { return obj.first; }
inline const std::string &NameFor(const System::Asteroid &obj) { return obj.Type() ? obj.Type()->Name() : obj.Name(); }
inline std::string NameFor(double d) {
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(3) << d;
	std::string str = std::move(stream).str();

	// Remove any trailing digits.
	while(str.back() == '0')
		str.pop_back();

	// Remove trailing dot.
	if(str.back() == '.')
		str.pop_back();
	return str;
}
template <typename T>
std::string NameFor(const WeightedItem<T> &item) { return NameFor(item.item); }
template <typename T>
std::string NameFor(const WeightedItem<T> *item) { return NameFor(item->item); }

template <typename T>
bool Count(const std::set<T> &container, const T &obj) { return container.count(obj); }
template <typename T>
bool Count(const std::vector<T> &container, const T &obj) { return std::find(container.begin(), container.end(), obj) != container.end(); }
template <typename T, typename U>
bool Count(const WeightedList<T> &list, const U &obj) { return std::find(list.begin(), list.end(), obj) != list.end(); }

template <typename T>
void Insert(std::set<T> &container, const T &obj) { container.insert(obj); }
template <typename T>
void Insert(std::vector<T> &container, const T &obj) { container.push_back(obj); }
template <typename T, typename U>
void Insert(WeightedList<T> &container, const U &obj) { container.emplace_back(obj.weight, obj.item); }

template <typename T>
void AdditionalCalls(DataWriter &writer, const T &obj) {}
inline void AdditionalCalls(DataWriter &writer, const System::Asteroid &obj)
{
	writer.WriteToken(obj.Count());
	writer.WriteToken(obj.Energy());
}
inline void AdditionalCalls(DataWriter &writer, const RandomEvent<Fleet> &obj)
{
	writer.WriteToken(obj.Period());
}
inline void AdditionalCalls(DataWriter &writer, const RandomEvent<Hazard> &obj)
{
	writer.WriteToken(obj.Period());
}
template <typename T>
inline void AdditionalCalls(DataWriter &writer, const WeightedItem<T> &item)
{
	if(item.weight != 1)
		writer.WriteToken(item.weight);
}

template <typename C>
void WriteDiff(DataWriter &writer, const char *name, const C &orig, const C *diff, bool onOneLine = false, bool allowRemoving = true, bool implicitAdd = false, bool sorted = false)
{
	auto writeRaw = onOneLine
		? [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const auto &element)
						{
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
				}
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const auto &element)
						{
							writer.WriteToken(name);
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
							writer.Write();
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(name);
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
					writer.Write();
				}
		};
	auto writeAdd = onOneLine
		? [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			writer.WriteToken("add");
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const auto &element)
						{
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
				}
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const auto &element)
						{
							writer.WriteToken("add");
							writer.WriteToken(name);
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
							writer.Write();
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken("add");
					writer.WriteToken(name);
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
					writer.Write();
				}
		};
	auto writeRemove = onOneLine
		? [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			writer.WriteToken("remove");
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const auto &element)
						{
							writer.Write(NameFor(element));
						});
			else
				for(auto &&it : list)
					writer.WriteToken(NameFor(it));
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const auto *lhs, const auto *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const auto &element)
						{
							writer.Write("remove", name, NameFor(element));
						});
			else
				for(auto &&it : list)
					writer.Write("remove", name, NameFor(it));
		};
	if(!diff)
	{
		if(!orig.empty())
			writeRaw(writer, name, orig, sorted);
		return;
	}

	typename std::decay<decltype(orig)>::type toAdd;
	auto toRemove = toAdd;

	for(auto &&it : orig)
		if(!Count(*diff, it))
			Insert(toAdd, it);
	for(auto &&it : *diff)
		if(!Count(orig, it))
			Insert(toRemove, it);

	if(toAdd.empty() && toRemove.empty())
		return;

	if(toRemove.size() == diff->size() && !diff->empty())
	{
		if(orig.empty())
			writer.Write("remove", name);
		else
			writeRaw(writer, name, toAdd, sorted);
	}
	else if(allowRemoving)
	{
		if(!toAdd.empty())
		{
			if(implicitAdd)
				writeRaw(writer, name, toAdd, sorted);
			else
				writeAdd(writer, name, toAdd, sorted);
		}
		if(!toRemove.empty())
			writeRemove(writer, name, toRemove, sorted);
	}
	else if(!orig.empty())
		writeRaw(writer, name, orig, sorted);
}



#endif
