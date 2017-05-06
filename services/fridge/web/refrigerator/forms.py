from django import forms
from . import models


class AddRefrigeratorForm(forms.ModelForm):
    class Meta:
        model = models.Refrigerator
        exclude = ('owner', 'image_id')


class AddFoodTypeForm(forms.ModelForm):
    class Meta:
        model = models.FoodType
        exclude = ('owner', )


class AddFoodToRefrigeratorForm(forms.ModelForm):
    class Meta:
        model = models.FoodItem
        exclude = ('refrigerator', )


class AddRecipeForm(forms.ModelForm):
    class Meta:
        model = models.Recipe
        exclude = ('owner', 'added_at')


class AddRecipeItemForm(forms.ModelForm):
    class Meta:
        model = models.RecipeItem
        exclude = ('recipe', )
