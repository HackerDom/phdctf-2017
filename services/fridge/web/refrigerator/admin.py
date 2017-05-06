from django.contrib import admin
from . import models


@admin.register(models.Refrigerator)
class RefrigeratorAdmin(admin.ModelAdmin):
    list_display = ('id', 'owner', 'title', 'description')
