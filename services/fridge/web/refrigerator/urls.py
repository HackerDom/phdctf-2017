from django.conf.urls import url
from . import views

urlpatterns = [
    url(r'^$', views.index, name='index'),

    url(r'^refrigerators/add/$', views.add_refrigerator,
        name='add_refrigerator'),
    url(r'^refrigerators/(?P<refrigerator_id>\d+)/$', views.view_refrigerator,
        name='refrigerator'),
    url(r'^refrigerators/(?P<refrigerator_id>\d+)/add/$',
        views.add_food_to_refrigerator, name='add_food_to_refrigerator'),
    
    url(r'^recipes/add/$', views.add_recipe, name='add_recipe'),
    url(r'^recipes/(?P<recipe_id>\d+)/$', views.view_recipe, name='recipe'),
    url(r'^recipes/(?P<recipe_id>\d+)/add/$', views.add_recipe_item,
        name='add_recipe_item'),

    url(r'^foods/add/$', views.add_food, name='add_food'),
]
