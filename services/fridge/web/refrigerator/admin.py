from django.contrib import admin
from django.contrib.auth.models import User, Group
from . import models


@admin.register(models.Refrigerator)
class RefrigeratorAdmin(admin.ModelAdmin):
    list_display = ('id', 'owner', 'title', 'description')


admin.site.unregister(User)
admin.site.unregister(Group)