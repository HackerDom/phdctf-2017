from django.contrib import admin
from . import models


@admin.register(models.Refrigerator)
class RefrigeratorAdmin(admin.ModelAdmin):
    list_display = ('id', 'owner', 'title', 'description')


@admin.register(models.FoodType)
class FoodTypeAdmin(admin.ModelAdmin):
    list_display = ('id', 'owner', 'name', 'description', 'unit')


@admin.register(models.FoodItem)
class FoodItemAdmin(admin.ModelAdmin):
    list_display = ('id', 'refrigerator', 'food_type', 'count')


@admin.register(models.Recipe)
class RecipeAdmin(admin.ModelAdmin):
    list_display = ('id', 'owner', 'added_at')


@admin.register(models.RecipeItem)
class RecipeItemAdmin(admin.ModelAdmin):
    list_display = ('id', 'recipe', 'food_type', 'count', 'what_to_do', 'pause_after')
