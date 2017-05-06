from django.contrib.auth import forms, views, login
from django.shortcuts import redirect
from django.views.decorators.http import require_POST


@require_POST
def registration(request):
    form = forms.UserCreationForm(data=request.POST)
    if form.is_valid():
        user = form.save()
        login(request, user)
        return redirect('index')
    return LoginView.as_view(extra_context={
        'registration_form': form
    })(request)


class LoginView(views.LoginView):
    extra_context = {
        'registration_form': forms.UserCreationForm()
    }
