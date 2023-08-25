// SPDX-License-Identifier: GPL-3.0

#ifndef IMGUI_EX_H_
#define IMGUI_EX_H_

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Set.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <rapidfuzz/fuzz.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>



namespace ImGui
{
	IMGUI_API bool InputDoubleEx(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputFloatEx(const char *label, float *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputDouble2Ex(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputInt64Ex(const char *label, int64_t *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputSizeTEx(const char *label, size_t *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool IsInputFocused(const char *id);

	template <typename T>
	IMGUI_API bool InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements, std::function<bool(const std::string &)> sort = {});
	template <typename T>
	IMGUI_API bool InputCombo(const char *label, std::string *input, const T **element, const Set<T> &elements, std::function<bool(const std::string &)> sort = {});

	IMGUI_API bool InputSwizzle(const char *label, int *swizzle, bool allowNoSwizzle = false);

	template <typename F>
	IMGUI_API void BeginSimpleModal(const char *id, const char *label, const char *button, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleNewModal(const char *id, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleRenameModal(const char *id, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleCloneModal(const char *id, F &&f);
}




template <typename T>
bool IsValid(const T &obj, ...)
{
	return !obj.Name().empty();
}
template <typename T>
bool IsValid(const T &obj, decltype(obj.TrueName(), void()) *)
{
	return !obj.TrueName().empty();
}



template <typename T>
IMGUI_API bool ImGui::InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements, std::function<bool(const std::string &)> sort)
{
	ImGuiWindow *window = GetCurrentWindow();
	const auto callback = [](ImGuiInputTextCallbackData *data)
	{
		bool &autocomplete = *reinterpret_cast<bool *>(data->UserData);
		switch(data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
			autocomplete = true;
			break;
		}

		return 0;
	};

	const auto id = ImHashStr("##combo/popup", 0, window->GetID(label));
	bool autocomplete = false;
	bool enter = false;
	if(InputText(label, input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, callback, &autocomplete))
	{
		SetActiveID(0, FindWindowByID(id));
		enter = true;
	}

	bool isOpen = IsPopupOpen(id, ImGuiPopupFlags_None);
	if(IsItemActive() && !isOpen)
	{
		OpenPopupEx(id, ImGuiPopupFlags_None);
		isOpen = true;
	}

	if(!isOpen)
		return false;
	if(autocomplete && input->empty())
		autocomplete = false;

    const ImRect bb(window->DC.CursorPos,
			window->DC.CursorPos + ImVec2(CalcItemWidth(), 0.f));

	bool changed = false;
	if(BeginComboPopup(id, bb, ImGuiComboFlags_None, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		if(IsWindowAppearing())
			BringWindowToDisplayFront(GetCurrentWindow());
		if(enter)
		{
			*element = elements.Find(*input) ? const_cast<T *>(elements.Get(*input)) : nullptr;
			CloseCurrentPopup();
			EndCombo();
			return true;
		}

		std::vector<std::pair<double, const char *>> weights;

		// Filter the possible values using the provider filter function (if available)
		// and perform a fuzzy search on the input to further limit the list.
		if(!input->empty())
		{
			rapidfuzz::fuzz::CachedPartialRatio<char> scorer(*input);
			const double scoreCutoff = .75;
			for(const auto &it : elements)
			{
				if(!IsValid(it.second, 0) || (sort && !sort(it.first)) || *input == it.first)
					continue;

				const double score = scorer.similarity(it.first, scoreCutoff);
				if(score > scoreCutoff)
					weights.emplace_back(score, it.first.c_str());
			}

			std::sort(weights.begin(), weights.end(),
					[](const auto &lhs, const auto &rhs)
					{
						if(lhs.first == rhs.first)
							return std::strcmp(lhs.second, rhs.second) < 0;
						return lhs.first > rhs.first;
					});
		}
		else
		{
			// If no input is provided sort the list by alphabetical order instead.
			for(const auto &it : elements)
				if(IsValid(it.second, 0) && (!sort || sort(it.first)))
					weights.emplace_back(0., it.first.c_str());
			std::sort(weights.begin(), weights.end(),
					[](const auto &lhs, const auto &rhs)
						{ return std::strcmp(lhs.second, rhs.second) < 0; });
		}

		if(!weights.empty())
		{
			auto topWeight = weights[0].first;
			for(const auto &item : weights)
			{
				// Allow the user to select an entry in the combo box.
				// This is a hack to workaround the fact that we change the focus when clicking an
				// entry and that this means that the filtered list will change (breaking entries).
				if(GetActiveID() == GetCurrentWindow()->GetID(item.second) || GetFocusID() == GetCurrentWindow()->GetID(item.second))
				{
					*element = const_cast<T *>(elements.Get(item.second));
					changed = true;
					*input = item.second;
					CloseCurrentPopup();
					SetActiveID(0, GetCurrentWindow());
				}

				if(topWeight && item.first < topWeight * .45)
					continue;

				if(Selectable(item.second) || autocomplete)
				{
					*element = const_cast<T *>(elements.Get(item.second));
					changed = true;
					*input = item.second;
					if(autocomplete)
					{
						autocomplete = false;
						CloseCurrentPopup();
						SetActiveID(0, GetCurrentWindow());
					}
				}
			}
		}
		EndCombo();
	}

	return changed;
}


template <typename T>
IMGUI_API bool ImGui::InputCombo(const char *label, std::string *input, const T **element, const Set<T> &elements, std::function<bool(const std::string &)> sort)
{
	return InputCombo(label, input, const_cast<T **>(element), elements, sort);
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleModal(const char *id, const char *label, const char *button, F &&f)
{
	if(BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if(IsWindowAppearing())
			SetKeyboardFocusHere();
		static std::string name;
		bool create = InputText(label, &name, ImGuiInputTextFlags_EnterReturnsTrue);
		if(Button("Cancel"))
		{
			CloseCurrentPopup();
			name.clear();
		}
		SameLine();
		if(name.empty())
			BeginDisabled();
		if((Button(button) || create) && !name.empty())
		{
			std::forward<F &&>(f)(name);
			CloseCurrentPopup();
			name.clear();
		}
		else if(name.empty())
			EndDisabled();
		EndPopup();
	}
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleNewModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "new name", "Create", std::forward<F &&>(f));
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleRenameModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "new name", "Rename", std::forward<F &&>(f));
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleCloneModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "clone name", "Clone", std::forward<F &&>(f));
}



#endif
