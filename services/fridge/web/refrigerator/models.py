from django.core import urlresolvers, validators
from django.db import models
from django.contrib.auth.models import User
import djchoices


class Refrigerator(models.Model):
    owner = models.ForeignKey(
        User,
        related_name='refrigerators',
        on_delete=models.CASCADE,
        help_text='Owner of the portable refrigerator',
    )

    title = models.CharField(
        max_length=100,
    )

    description = models.TextField()

    image_id = models.PositiveIntegerField()

    def get_absolute_url(self):
        return urlresolvers.reverse('refrigerator', args=(self.id, ))

    def __str__(self):
        return self.title


class Unit(djchoices.DjangoChoices):
    Gram = djchoices.ChoiceItem(value=1)
    Kilogram = djchoices.ChoiceItem(value=2)
    Liter = djchoices.ChoiceItem(value=3)
    Box = djchoices.ChoiceItem(value=4)
    Package = djchoices.ChoiceItem(value=5)


Unit.do_not_call_in_templates = True


class FoodType(models.Model):
    owner = models.ForeignKey(
        User,
        related_name='food_types',
        on_delete=models.CASCADE,
        help_text='Owner of the food type',
    )

    name = models.CharField(
        max_length=100,
        help_text='E.g., potatoes, milk or carrot',
    )

    description = models.TextField(
        help_text='Detailed information about this food type'
    )

    unit = models.PositiveIntegerField(
        choices=Unit.choices,
        validators=[Unit.validator]
    )

    def __str__(self):
        return self.name


class FoodItem(models.Model):
    refrigerator = models.ForeignKey(
        Refrigerator,
        related_name='items',
        on_delete=models.CASCADE,
        help_text='Refrigerator stored this food item'
    )

    food_type = models.ForeignKey(
        FoodType,
        related_name='+',
        on_delete=models.CASCADE,
    )

    count = models.PositiveIntegerField()

    class Meta:
        unique_together = ('refrigerator', 'food_type')


class Recipe(models.Model):
    owner = models.ForeignKey(
        User,
        related_name='recipes',
        on_delete=models.CASCADE,
    )

    title = models.CharField(
        max_length=200,
    )

    description = models.TextField()

    added_at = models.DateTimeField(
        auto_now_add=True,
        db_index=True,
    )

    def __str__(self):
        return self.title

    def get_absolute_url(self):
        return urlresolvers.reverse('recipe', args=(self.id, ))


class RecipeItem(models.Model):
    recipe = models.ForeignKey(
        Recipe,
        related_name='items',
        on_delete=models.CASCADE,
    )

    food_type = models.ForeignKey(
        FoodType,
        related_name='+',
        on_delete=models.CASCADE,
    )

    what_to_do = models.TextField()

    count = models.PositiveIntegerField(
        help_text='In units for this food',
        validators=[validators.MinValueValidator(
            1,
            'You should use at least one unit of food'
        )]
    )

    pause_after = models.PositiveIntegerField(
        help_text='In seconds'
    )

    class Meta:
        unique_together = ('recipe', 'food_type')
