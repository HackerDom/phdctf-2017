import random

from django.db import transaction
from django.db.models import Sum
from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth.decorators import login_required
from django.views.decorators.http import require_POST

from . import models
from . import forms


def index(request,
          add_refrigerator_form=None,
          add_recipe_form=None,
          add_food_type_form=None):
    if request.user.is_authenticated:
        refrigerators = list(request.user.refrigerators.all())
        recipes = list(request.user.recipes.all())

        if add_refrigerator_form is None:
            add_refrigerator_form = forms.AddRefrigeratorForm()
        if add_recipe_form is None:
            add_recipe_form = forms.AddRecipeForm()
        if add_food_type_form is None:
            add_food_type_form = forms.AddFoodTypeForm()

        return render(request, 'refrigerator/index.html', {
            'refrigerators': refrigerators,
            'recipes': recipes,
            'add_refrigerator_form': add_refrigerator_form,
            'add_recipe_form': add_recipe_form,
            'add_food_type_form': add_food_type_form,
        })

    return redirect('login')


@login_required
@require_POST
def add_refrigerator(request):
    form = forms.AddRefrigeratorForm(data=request.POST)
    if form.is_valid():
        refrigerator = form.save(commit=False)
        refrigerator.owner = request.user
        refrigerator.image_id = random.randint(1, 10)
        refrigerator.save()
        return redirect(refrigerator)

    return index(request, add_refrigerator_form=form)


@login_required
def view_refrigerator(request, refrigerator_id, add_food_to_refrigerator_form=None):
    refrigerator = get_object_or_404(
        models.Refrigerator,
        pk=refrigerator_id,
        owner=request.user,
    )

    if add_food_to_refrigerator_form is None:
        add_food_to_refrigerator_form = forms.AddFoodToRefrigeratorForm()
        add_food_to_refrigerator_form.fields['food_type'].queryset = \
            models.FoodType.objects.filter(owner=request.user)

    return render(request, 'refrigerator/refrigerator.html', {
        'refrigerator': refrigerator,
        'add_food_to_refrigerator_form': add_food_to_refrigerator_form,
        'Unit': models.Unit,
    })


@login_required
@require_POST
def add_food_to_refrigerator(request, refrigerator_id):
    refrigerator = get_object_or_404(
        models.Refrigerator,
        pk=refrigerator_id,
        owner=request.user,
    )

    form = forms.AddFoodToRefrigeratorForm(data=request.POST)
    if form.is_valid():
        food_item = form.save(commit=False)
        food_item.refrigerator = refrigerator

        with transaction.atomic():
            old_items = refrigerator.items.filter(food_type=food_item.food_type)
            old_count = old_items.aggregate(count=Sum('count'))['count']
            old_items.delete()

            if old_count is not None:
                food_item.count += old_count
            food_item.save()

        return redirect(refrigerator)

    return view_refrigerator(request, refrigerator_id, form)


@login_required
@require_POST
def add_recipe(request):
    form = forms.AddRecipeForm(data=request.POST)
    if form.is_valid():
        recipe = form.save(commit=False)
        recipe.owner = request.user
        recipe.save()
        return redirect(recipe)

    return index(request, add_recipe_form=form)


@login_required
def view_recipe(request, recipe_id, add_recipe_item_form=None):
    recipe = get_object_or_404(models.Recipe, pk=recipe_id, owner=request.user)

    if add_recipe_item_form is None:
        add_recipe_item_form = forms.AddRecipeItemForm()
        add_recipe_item_form.fields['food_type'].queryset = \
            models.FoodType.objects.filter(owner=request.user)

    return render(request, 'refrigerator/recipe.html', {
        'recipe': recipe,
        'add_recipe_item_form': add_recipe_item_form,
        'Unit': models.Unit,
    })


@login_required
@require_POST
def add_recipe_item(request, recipe_id):
    recipe = get_object_or_404(models.Recipe, pk=recipe_id, owner=request.user)

    form = forms.AddRecipeItemForm(data=request.POST)
    if form.is_valid():
        recipe_item = form.save(commit=False)
        recipe_item.recipe = recipe
        recipe_item.save()
        return redirect(recipe)

    return view_recipe(request, recipe_id, add_recipe_item_form=form)


@login_required
@require_POST
def add_food(request):
    form = forms.AddFoodTypeForm(data=request.POST)
    if form.is_valid():
        food_type = form.save(commit=False)
        food_type.owner = request.user
        food_type.save()
        return redirect('index')

    return index(request, add_food_type_form=form)
